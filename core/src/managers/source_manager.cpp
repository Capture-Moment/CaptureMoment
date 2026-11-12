/**
 * @file source_manager.cpp
 * @brief Implementation of SourceManager
 * @author CaptureMoment Team
 * @date 2025
 */

#include "managers/source_manager.h"
#include "image_config/raw_settings.h"
#include "image_config/heic_settings.h"
#include "utils/color_space_utils.h"

#include <OpenImageIO/imagebufalgo.h>
#include <OpenImageIO/imageio.h>
#include <spdlog/spdlog.h>
#include <mutex>
#include <algorithm>

namespace CaptureMoment::Core::Managers {

// Global static members for cache management
static std::shared_ptr<OIIO::ImageCache> s_global_cache_ptr { nullptr };
static std::once_flag s_cache_init_flag;

/**
 * @brief Checks if a file path has one of the specified extensions.
 * @param path The file path to check.
 * @param extensions The list of extensions to compare against.
 * @return true If the file has one of the specified extensions.
 * @return false If the file does not have any of the specified extensions.
 */
[[nodiscard]] static bool hasExtension(std::string_view path, std::span<const std::string_view> extensions) noexcept
{
    std::string lower_path(path);
    std::transform(lower_path.begin(), lower_path.end(), lower_path.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    for (const auto& ext : extensions)
    {
        if (lower_path.size() >= ext.size() &&
            lower_path.compare(lower_path.size() - ext.size(), ext.size(), ext) == 0) {
            return true;
        }
    }
    return false;
}

OIIO::ImageCache* SourceManager::getGlobalCache()
{
    std::call_once(s_cache_init_flag, []() {
        s_global_cache_ptr = OIIO::ImageCache::create();

        if (s_global_cache_ptr) {
            s_global_cache_ptr->attribute("max_memory_MB", 2048.0f);
            spdlog::debug("[SourceManager::getGlobalCache]: OIIO ImageCache created: 2GB limit");
        } else {
            spdlog::critical("[SourceManager::getGlobalCache]: Failed to create OIIO ImageCache");
        }
    });
    return s_global_cache_ptr.get();
}

SourceManager::SourceManager()
    : m_cache(getGlobalCache())
{
    if (!m_cache) {
        spdlog::critical("[SourceManager::SourceManager]: Failed to get global ImageCache");
        throw std::runtime_error("SourceManager: Initialization failed");
    }
}

SourceManager::~SourceManager()
{
    unloadInternal();
}

std::expected<void, ErrorHandling::CoreError> SourceManager::loadFile(std::string_view path)
{
    if (path.empty()) {
        spdlog::warn("[SourceManager::loadFile]: Empty file path");
        return std::unexpected(ErrorHandling::CoreError::FileNotFound);
    }

    std::scoped_lock<std::mutex> lock(m_mutex);

    if (isLoaded_unsafe()) {
        unloadInternal();
    }

    spdlog::info("[SourceManager::loadFile]: Loading: '{}'", path);

    // Dispatch based on file type
    if (isRawFile(path)) {
        return loadRawFile(path);
    } else if (isHeicFile(path)) {
        return loadHeicFile(path);
    }

    return loadStandardFile(path);
}

bool SourceManager::isRawFile(std::string_view path) const
{
    static const std::array<std::string_view, 25> raw_extensions = {
        ".nef", ".cr2", ".cr3", ".arw", ".dng", ".raw", ".rw2",
        ".orf", ".sr2", ".raf", ".mrw", ".pef", ".srw", ".x3f",
        ".3fr", ".iiq", ".kdc", ".mef", ".nrw", ".srw", ".crw",
        ".mos", ".erf", ".dc2", ".dcr"
    };

    return hasExtension(path, raw_extensions);
}

bool SourceManager::isHeicFile(std::string_view path) const
{
    static const std::array<std::string_view, 3> heic_extensions = {
        ".heic", ".heif", ".avif"
    };

    return hasExtension(path, heic_extensions);
}

std::expected<void, ErrorHandling::CoreError> SourceManager::loadRawFile(std::string_view path)
{
    spdlog::debug("[SourceManager::loadRawFile]: Processing RAW file: {}", path);

    // ============================================================
    // Configure OIIO/LibRaw with optimized settings
    // ============================================================
    ImageConfig::Raw::RawSettings settings;
    settings.set_demosaic(ImageConfig::Raw::DemosaicAlgorithm::amaze);
    settings.set_color_space(ImageConfig::Raw::RawColorSpace::prophoto_linear);
    settings.set_highlight_mode(ImageConfig::Raw::HighlightMode::blend);
    settings.set_balance_clamped(true);
    settings.set_use_camera_wb(true);
    settings.set_fbdd_noiserd(ImageConfig::Raw::FbddNoiseRd::light);
    settings.set_camera_matrix(ImageConfig::Raw::CameraMatrixMode::always);

    OIIO::ImageSpec config;
    config.attribute("raw:Demosaic", settings.get_demosaic_string());
    config.attribute("raw:ColorSpace", settings.get_color_space_string());
    config.attribute("raw:HighlightMode", settings.get_highlight_mode_value());
    config.attribute("raw:balance_clamped", static_cast<int>(settings.get_balance_clamped()));
    config.attribute("raw:use_camera_matrix", settings.get_camera_matrix_value());
    config.attribute("raw:use_camera_wb", static_cast<int>(settings.get_use_camera_wb()));
    config.attribute("raw:use_auto_wb", static_cast<int>(settings.get_use_auto_wb()));
    config.attribute("raw:fbdd_noiserd", settings.get_fbdd_noiserd_value());

    // ============================================================
    // Load via OIIO ImageCache
    // ============================================================
    auto buf_result { loadImageBuffer(path, &config) };
    if (!buf_result) {
        spdlog::error("[SourceManager::loadRawFile]: Failed to load RAW file '{}': {}", path, buf_result.error());
        return std::unexpected(buf_result.error());
    }

    // ============================================================
    // Convert and store
    // ============================================================
    m_current_path = path;
    auto result { convertToRgbaInternal(std::move(buf_result.value())) };

    if (result) {
        spdlog::info("[SourceManager::loadRawFile]: Loaded '{}': {}x{} RGBA_F32",
                     path, m_image_buf->spec().width, m_image_buf->spec().height);
    }

    return result;
}

std::expected<void, ErrorHandling::CoreError> SourceManager::loadHeicFile(std::string_view path)
{
    spdlog::debug("[SourceManager::loadHeicFile]: Processing HEIC file: {}", path);

    // ============================================================
    // Configure OIIO/libheif with HEIC settings
    // ============================================================
    ImageConfig::Heic::HeicSettings settings;

    OIIO::ImageSpec config;
    config.attribute("oiio:reorient", settings.get_reorient_value());
    config.attribute("oiio:UnassociatedAlpha", settings.get_unassociated_alpha_value());

    // ============================================================
    // Load via OIIO ImageCache
    // ============================================================
    auto buf_result { loadImageBuffer(path, &config) };
    if (!buf_result) {
        spdlog::error("[SourceManager::loadHeicFile]: Failed to load HEIC file '{}': {}", path, buf_result.error());
        return std::unexpected(buf_result.error());
    }

    // ============================================================
    // Convert to linear (HEIC from smartphones are typically sRGB)
    // ============================================================
    auto [is_linear, cs_name] { Utils::analyzeColorSpace(buf_result->spec()) };

    if (!is_linear && !cs_name.empty())
    {
        auto conversion { Utils::transformToColorSpace(*buf_result, "lin_rec709_scene") };
        if (!conversion) {
            spdlog::error("[SourceManager::loadHeicFile]: Color space conversion failed for '{}': {}", path, conversion.error());
            return std::unexpected(conversion.error());
        }

    }
    // ============================================================
    // Convert and store
    // ============================================================
    m_current_path = path;
    auto result { convertToRgbaInternal(std::move(buf_result.value())) };

    if (result) {
        spdlog::info("[SourceManager::loadHeicFile]: Loaded '{}': {}x{} RGBA_F32",
                     path, m_image_buf->spec().width, m_image_buf->spec().height);
    }

    return result;
}

std::expected<void, ErrorHandling::CoreError> SourceManager::loadStandardFile(std::string_view path)
{
    spdlog::debug("[SourceManager::loadStandardFile]: Processing standard image: {}", path);

    // ============================================================
    // Load via OIIO ImageCache (no special config)
    // ============================================================
    auto buf_result { loadImageBuffer(path, nullptr) };
    if (!buf_result) {
        spdlog::error("[SourceManager::loadStandardFile]: Failed to load standard file '{}': {}", path, buf_result.error());
        return std::unexpected(buf_result.error());
    }

    // ============================================================
    // Convert to linear if needed
    // ============================================================
    auto [is_linear, cs_name] { Utils::analyzeColorSpace(buf_result->spec()) };

    if (!is_linear && !cs_name.empty())
    {
        auto conversion { Utils::transformToColorSpace(*buf_result, "lin_rec709_scene") };
        if (!conversion) {
            spdlog::error("[SourceManager::loadStandardFile]: Color space conversion failed for '{}': {}", path, conversion.error());
            return std::unexpected(conversion.error());
        }

        auto [is_linear, cs_name] { Utils::analyzeColorSpace(buf_result->spec()) };

        spdlog::debug("[SourceManager::loadStandardFile]: is linear: '{}' new space: '{}' ",
                      is_linear, cs_name);
    }

    // ============================================================
    // Convert and store
    // ============================================================
    m_current_path = path;
    auto result { convertToRgbaInternal(std::move(buf_result.value())) };

    if (result) {
        spdlog::info("[SourceManager::loadStandardFile]: Loaded '{}': {}x{} RGBA_F32",
                     path, m_image_buf->spec().width, m_image_buf->spec().height);
    }

    return result;
}

std::expected<OIIO::ImageBuf, ErrorHandling::CoreError>
SourceManager::loadImageBuffer(std::string_view path, const OIIO::ImageSpec* config)
{
    if (config)
    {
        // Config path: ImageInput::open applies attributes (RAW, HEIF)
        auto in { OIIO::ImageInput::open(std::string(path), config) };
        if (!in) {
            spdlog::error("[SourceManager::loadImageBuffer]: Failed to open '{}': {}", path, OIIO::geterror());
            return std::unexpected(ErrorHandling::CoreError::DecodingError);
        }

        OIIO::ImageSpec float_spec { in->spec() };
        float_spec.set_format(OIIO::TypeDesc::FLOAT);

        OIIO::ImageBuf buf(float_spec);

        if (!in->read_image(0, 0, 0, -1, OIIO::TypeDesc::FLOAT, buf.localpixels())) {
            spdlog::error("[SourceManager::loadImageBuffer]: Failed to read '{}': {}", path, in->geterror());
            return std::unexpected(ErrorHandling::CoreError::DecodingError);
        }

        in->close();
        return buf;
    }

    // Standard path: ImageCache
    OIIO::ImageBuf buf(std::string(path), 0, 0, s_global_cache_ptr);
    if (!buf.read(0, 0, true, OIIO::TypeDesc::FLOAT)) {
        spdlog::error("[SourceManager::loadImageBuffer]: Failed to read '{}': {}", path, buf.geterror());
        return std::unexpected(ErrorHandling::CoreError::DecodingError);
    }

    return buf;
}

std::expected<void, ErrorHandling::CoreError> SourceManager::convertToRgbaInternal(OIIO::ImageBuf&& src_buf)
{
    // ============================================================
    // Get dimensions
    // ============================================================
    const Common::ImageDim w { static_cast<Common::ImageDim>(src_buf.spec().width) };
    const Common::ImageDim h { static_cast<Common::ImageDim>(src_buf.spec().height) };
    const Common::ImageChan src_ch { static_cast<Common::ImageChan>(src_buf.spec().nchannels) };

    spdlog::debug("[SourceManager::convertToRgbaInternal]: {}x{} ({} channels)", w, h, src_ch);

    // ============================================================
    // Convert to RGBA_F32
    // ============================================================
    OIIO::ImageSpec target_spec(static_cast<int>(w), static_cast<int>(h), 4, OIIO::TypeDesc::FLOAT);
    OIIO::ImageBuf rgba_buf(target_spec);

    std::vector<int> channel_order = {0, 1, 2, 3};
    std::vector<float> channel_values = {0.0f, 0.0f, 0.0f, 1.0f};

    switch (src_ch)
    {
    case 1:  channel_order = {0, 0, 0, -1}; break;
    case 2:  channel_order = {0, 0, 0, 1}; break;
    case 3:  channel_order = {0, 1, 2, -1}; break;
    default: channel_order = {0, 1, 2, 3}; break;
    }

    if (!OIIO::ImageBufAlgo::channels(rgba_buf, src_buf, 4, channel_order, channel_values)) {
        spdlog::error("[SourceManager::convertToRgbaInternal]: Channel conversion failed: {}", rgba_buf.geterror());
        return std::unexpected(ErrorHandling::CoreError::AllocationFailed);
    }

    // ============================================================
    // Store final buffer
    // ============================================================
    m_image_buf = std::make_unique<OIIO::ImageBuf>(std::move(rgba_buf));

    return {};
}

void SourceManager::unload()
{
    std::scoped_lock<std::mutex> lock(m_mutex);
    unloadInternal();
}

void SourceManager::unloadInternal()
{
    if (m_image_buf && m_image_buf->initialized()) {
        spdlog::debug("[SourceManager::unloadInternal]: Unloading '{}'", m_current_path);
    }
    m_image_buf.reset();
    m_current_path.clear();
}

bool SourceManager::isLoaded() const
{
    std::scoped_lock<std::mutex> lock(m_mutex);
    return isLoaded_unsafe();
}

bool SourceManager::isLoaded_unsafe() const
{
    return m_image_buf && m_image_buf->initialized();
}

Common::ImageDim SourceManager::width() const noexcept
{
    return m_image_buf ? static_cast<Common::ImageDim>(m_image_buf->spec().width) : 0;
}

Common::ImageDim SourceManager::height() const noexcept
{
    return m_image_buf ? static_cast<Common::ImageDim>(m_image_buf->spec().height) : 0;
}

Common::ImageChan SourceManager::channels() const noexcept
{
    return m_image_buf ? static_cast<Common::ImageChan>(m_image_buf->spec().nchannels) : 0;
}

std::expected<std::unique_ptr<Common::ImageRegion>, ErrorHandling::CoreError>
SourceManager::getTile(Common::ImageDim x, Common::ImageDim y,
                       Common::ImageDim width, Common::ImageDim height)
{
    std::scoped_lock<std::mutex> lock(m_mutex);

    if (!isLoaded_unsafe()) {
        spdlog::warn("[SourceManager::getTile] No image loaded");
        return std::unexpected(ErrorHandling::CoreError::SourceNotLoaded);
    }

    if (width == 0 || height == 0) {
        return std::unexpected(ErrorHandling::CoreError::InvalidImageRegion);
    }

    // Bounds contract: reject requests sharing NO pixel with the image.
    // ImageDim is UNSIGNED and zero sizes are already rejected above, so a
    // request can only miss the image on the right/bottom side: these two
    // comparisons fully express "no intersection".
    const auto img_width  { static_cast<Common::ImageDim>(m_image_buf->spec().width) };
    const auto img_height { static_cast<Common::ImageDim>(m_image_buf->spec().height) };

    if (x >= img_width || y >= img_height) {
        return std::unexpected(ErrorHandling::CoreError::InvalidImageRegion);
    }

    // OIIO handles partial intersections via ROI - no manual clamping needed
    OIIO::ROI roi(
        static_cast<int>(x), static_cast<int>(x + width),
        static_cast<int>(y), static_cast<int>(y + height),
        0, 1,
        0, 4
        );

    // Allocate result (zero-initialized: the outside-intersection area
    // of clamped requests is filled by this, not by OIIO)
    const size_t data_size { width * height * 4 };
    std::vector<float> data(data_size);

    if (!m_image_buf->get_pixels(roi, OIIO::TypeDesc::FLOAT, data.data())) {
        spdlog::warn("[SourceManager::getTile]: get_pixels failed: {}", m_image_buf->geterror());
        return std::unexpected(ErrorHandling::CoreError::IOError);
    }

    // Create ImageRegion
    auto region { std::make_unique<Common::ImageRegion>(
        std::move(data),
        width,
        height,
        static_cast<Common::ImageChan>(4)
        )};

    region->m_x = static_cast<Common::ImageCoord>(x);
    region->m_y = static_cast<Common::ImageCoord>(y);

    return region;
}

std::expected<void, ErrorHandling::CoreError>
SourceManager::setTile(const Common::ImageRegion& tile)
{
    std::scoped_lock<std::mutex> lock(m_mutex);

    if (!isLoaded_unsafe()) {
        spdlog::warn("[SourceManager::setTile] No image loaded");
        return std::unexpected(ErrorHandling::CoreError::SourceNotLoaded);
    }

    if (!tile.isValid()) {
        return std::unexpected(ErrorHandling::CoreError::InvalidImageRegion);
    }

    if (tile.m_format != Common::PixelFormat::RGBA_F32 || tile.m_channels != 4) {
        spdlog::error("[SourceManager::setTile] Invalid format: expected RGBA_F32");
        return std::unexpected(ErrorHandling::CoreError::InvalidImageRegion);
    }

    // Bounds contract: reject regions sharing NO pixel with the image.
    // ImageRegion mixes SIGNED coordinates (ImageCoord = int32, negative
    // offsets allowed) with UNSIGNED dimensions (ImageDim = size_t): raw
    // arithmetic like `m_x + m_width` silently converts the negative side
    // to size_t (e.g. -3 becomes ~1.8e19) and the check never fires.
    // Everything is normalized to int, where the math keeps its ordinary
    // meaning; the same locals feed the OIIO ROI (int fields).
    const int img_width  { m_image_buf->spec().width };
    const int img_height { m_image_buf->spec().height };
    const int tile_x { tile.m_x };
    const int tile_y { tile.m_y };
    const int tile_w { static_cast<int>(tile.m_width) };
    const int tile_h { static_cast<int>(tile.m_height) };

    const bool is_no_intersection {
        tile_x >= img_width
        || tile_y >= img_height
        || tile_x + tile_w <= 0
        || tile_y + tile_h <= 0
    };
    if (is_no_intersection) {
        return std::unexpected(ErrorHandling::CoreError::InvalidImageRegion);
    }

    // OIIO handles partial intersections via ROI - no manual clamping needed
    const OIIO::ROI roi(tile_x, tile_x + tile_w, tile_y, tile_y + tile_h, 0, 1, 0, 4);

    if (!m_image_buf->set_pixels(roi, OIIO::TypeDesc::FLOAT, tile.m_data.data())) {
        spdlog::error("[SourceManager::setTile] set_pixels failed: {}", m_image_buf->geterror());
        return std::unexpected(ErrorHandling::CoreError::IOError);
    }

    return {};
}

std::optional<std::string> SourceManager::getMetadata(std::string_view key) const
{
    std::scoped_lock<std::mutex> lock(m_mutex);

    if (!m_image_buf) {
        return std::nullopt;
    }

    const auto& spec { m_image_buf->spec() };
    const auto* attr { spec.find_attribute(std::string(key)) };

    return attr ? std::optional{attr->get_string()} : std::nullopt;
}

std::string SourceManager::getImageSourcePath() const
{
    std::scoped_lock lock(m_mutex);
    return m_current_path;
}

} // namespace CaptureMoment::Core::Managers
