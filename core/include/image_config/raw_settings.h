/**
 * @file raw_settings.h
 * @brief Comprehensive RAW development settings with runtime configuration.
 * @author CaptureMoment Team
 * @date 2026
 * This file provides a complete configuration system for RAW image development
 * via OIIO/LibRaw. All parameters are runtime-configurable with sensible defaults.
 * @note Uses magic_enum for enum - string conversion.
 * @note All getters are constexpr and noexcept.
 * @note All setters are noexcept.
 */
#pragma once
#include <string>
#include <array>
#include <magic_enum/magic_enum.hpp>
namespace CaptureMoment::Core::ImageConfig {

/**
 * @brief Namespace for RAW-specific configuration settings.
 */
namespace Raw {
// ═══════════════════════════════════════════════════════════════════════════════
// ENUMERATIONS FOR RAW SETTINGS
// ═══════════════════════════════════════════════════════════════════════════════
/**
 * @enum HighlightMode
 * @brief Controls how overexposed/clipped highlights are handled during RAW development.
 * When sensor channels saturate at different levels, highlights can take on 
 * unnatural color casts. The highlight mode determines how these clipped values
 * are reconstructed or blended.
 */
enum class HighlightMode : std::uint8_t {
    clip = 0,       ///< Clip all channels to maximum (default LibRaw behavior). Fast but may cause color shifts.
    unclip = 1,     ///< Unclip: attempt to reconstruct clipped values. Better for slight overexposure.
    blend = 2,      ///< Blend clipped areas with surrounding unclipped data. Natural-looking highlights.
    rebuild = 3     ///< Reconstruct highlights using advanced algorithms. Best quality for heavily clipped areas.
};

/**
 * @enum DemosaicAlgorithm
 * @brief Demosaicing algorithms for converting Bayer pattern RAW data to RGB.
 * Demosaicing is the process of interpolating missing color values from the
 * Bayer filter pattern. Quality vs speed trade-offs exist between algorithms.
 */
enum class DemosaicAlgorithm : std::uint8_t {
    linear,     ///< Simple bilinear interpolation. Fast, lower quality.
    vng,        ///< Variable Number of Gradients. Good balance of speed/quality.
    ppg,        ///< Patterned Pixel Grouping. Moderate quality.
    ahd,        ///< Adaptive Homogeneity-Directed. Good quality, standard default.
    dcb,        ///< Directional Color Filter Array interpolation with Bilateral filter.
    ahd_mod,    ///< Modified AHD algorithm.
    afd,        ///< Adaptive Frequency Domain demosaicing.
    vcd,        ///< Variance of Color Difference.
    mixed,      ///< Mixed demosaicing approach.
    lmmse,      ///< Linear Minimum Mean Square Error.
    amaze,      ///< Aliasing Minimization and Zipper Elimination.
    dht,        ///< DHT-based demosaicing.
    aahd,       ///< Advanced Adaptive Homogeneity-Directed.
    none        ///< No demosaicing (return raw Bayer pattern).
};

/**
 * @enum RawColorSpace
 * @brief Color primaries and transfer function for RAW development output.
 * The working color space determines the gamut of colors that can be represented
 * and affects how edits are performed. Linear spaces are preferred for editing.
 */
enum class RawColorSpace : std::uint8_t {
    raw,            ///< Raw sensor data, no color space conversion.
    srgb,           ///< sRGB with gamma curve. Not recommended for editing.
    srgb_linear,    ///< sRGB primaries with linear transfer. Good for editing.
    adobe,          ///< Adobe RGB (1998). Wider gamut than sRGB.
    wide,           ///< Wide gamut color space.
    prophoto,       ///< ProPhoto RGB with gamma. Very wide gamut.
    prophoto_linear,///< ProPhoto primaries with linear transfer. Best for editing (recommended).
    xyz,            ///< CIE XYZ connection space.
    aces,           ///< Academy Color Encoding System. Film industry standard.
    dci_p3,         ///< DCI-P3 color space (LibRaw >= 0.21).
    rec2020         ///< ITU-R BT.2020 color space (LibRaw >= 0.20).
};

/**
 * @enum CameraMatrixMode
 * @brief Controls when to use the embedded camera color profile matrix.
 * Camera color matrices provide accurate color reproduction by converting
 * from camera-specific color space to a standard working space.
 */
enum class CameraMatrixMode : std::uint8_t {
    never = 0,      ///< Never use embedded color matrix.
    dng_only = 1,   ///< Use only for DNG files (LibRaw default).
    always = 3      ///< Always use embedded color matrix when available (recommended).
};

/**
 * @enum FbddNoiseRd
 * @brief FBDD (Fuzzy Block Denoising and Demosaicing) noise reduction level.
 * Controls noise reduction applied BEFORE demosaicing. This can significantly
 * improve demosaicing quality for noisy images, especially with AMaZE algorithm.
 */
enum class FbddNoiseRd : std::uint8_t {
    off = 0,    ///< No FBDD noise reduction.
    light = 1,  ///< Light FBDD reduction (recommended for most images).
    full = 2    ///< Full FBDD reduction (for very noisy images).
};

/**
 * @enum Orientation
 * @brief Image orientation values matching EXIF orientation codes.
 * Used to rotate/flip the image during RAW development based on camera
 * orientation metadata or user preference.
 */
enum class Orientation : std::int8_t {
    ignored = -1,       ///< Ignore orientation metadata.
    normal = 0,         ///< Normal orientation (0°).
    flip_horizontal = 1, ///< Mirror horizontal.
    rotate_180 = 2,      ///< Rotate 180°.
    flip_vertical = 3,   ///< Mirror vertical.
    flip_h_rotate_90 = 4,  ///< Mirror horizontal and rotate 90° CW.
    rotate_90_cw = 5,     ///< Rotate 90° clockwise.
    flip_h_rotate_270 = 6, ///< Mirror horizontal and rotate 270° CW.
    rotate_270_cw = 7,    ///< Rotate 270° clockwise (or 90° CCW).
    flip_v_rotate_90 = 8   ///< Mirror vertical and rotate 90° CW.
};

/**
 * @enum WhiteBalanceMode
 * @brief White balance calculation mode for RAW development.
 * Determines how the initial white balance multipliers are calculated.
 * Camera WB is typically the most accurate starting point.
 * @note This is a high-level abstraction. OIIO uses two separate flags
 *       (use_camera_wb, use_auto_wb) with precedence rules.
 */
enum class WhiteBalanceMode : std::uint8_t {
    none = 0,       ///< No white balance applied.
    camera = 1,     ///< Use camera-recorded white balance (recommended default).
    auto_wb = 2,    ///< Calculate white balance automatically from image data.
    grey_box = 3,   ///< Calculate from a user-specified grey card region.
    user_mul = 4    ///< Use user-specified custom white balance multipliers.
};

// ═══════════════════════════════════════════════════════════════════════════════
// RAW SETTINGS STRUCTURE
// ═══════════════════════════════════════════════════════════════════════════════
/**
 * @struct RawSettings
 * @brief Complete configuration for RAW image development via OIIO/LibRaw.
 * This struct encapsulates all RAW processing parameters with sensible defaults
 * optimized for quality. Parameters can be modified at runtime via setters.
 * ## Architecture Notes
 * - **Application-fixed settings**: color_space, demosaic, highlight_mode, balance_clamped
 *   These are optimized for quality and not exposed to users directly.
 * - **Potential user controls**: exposure, user_black, user_sat (via OperationDescriptor)
 *   Users interact with these through the operations system.
 * ## Default Configuration Rationale
 * - **Demosaic**: amaze for best quality (slower but superior edge handling)
 * - **ColorSpace**: prophoto_linear for widest editing gamut with linear response
 * - **HighlightMode**: blend for natural highlight recovery without artifacts
 * - **balance_clamped**: Enabled to prevent color shifts in clipped highlights
 * - **use_camera_matrix**: always (3) for accurate color reproduction
 * - **use_camera_wb**: Enabled for correct initial white balance
 * - **fbdd_noiserd**: light for improved amaze quality with minimal detail loss
 * @note Thread-safe for read access. Write operations should be synchronized externally.
 */
struct RawSettings {
    // ═════════════════════════════════════════════════════════════════════════
    // DEMOSAIC SETTINGS
    // ═════════════════════════════════════════════════════════════════════════
private:
    /** @brief Demosaicing algorithm to use for RAW development.
     * The default is AMaZE, which provides the best quality with minimal artifacts,
     * especially around edges and fine details. However, it is also the slowest
     * algorithm. For faster performance at the cost of quality, users can switch
     * to linear or VNG algorithms.
    */
    DemosaicAlgorithm m_demosaic{DemosaicAlgorithm::amaze};

    /** @brief Flag indicating whether to output at half resolution.
     * When true, the output will be at half the original resolution,
     * which can improve loading performance but at the cost of detail.
     */
    bool m_half_size{false};

public:
    /**
     * @brief Gets the demosaicing algorithm.
     * @return The current demosaicing algorithm.
     */
    [[nodiscard]] constexpr DemosaicAlgorithm get_demosaic() const noexcept { return m_demosaic; }

    /**
     * @brief Sets the demosaicing algorithm.
     * @param algorithm The demosaicing algorithm to use.
     */
    constexpr void set_demosaic(DemosaicAlgorithm algorithm) noexcept { m_demosaic = algorithm; }

    /**
     * @brief Gets the demosaic algorithm name as string (for OIIO attribute).
     * @return The algorithm name string (e.g., "AMaZE").
     * @note Converts enum name to OIIO format.
     */
    [[nodiscard]] std::string get_demosaic_string() const noexcept {
        switch (m_demosaic) {
            case DemosaicAlgorithm::linear:  return "linear";
            case DemosaicAlgorithm::vng:     return "VNG";
            case DemosaicAlgorithm::ppg:     return "PPG";
            case DemosaicAlgorithm::ahd:     return "AHD";
            case DemosaicAlgorithm::dcb:     return "DCB";
            case DemosaicAlgorithm::ahd_mod: return "AHD-Mod";
            case DemosaicAlgorithm::afd:     return "AFD";
            case DemosaicAlgorithm::vcd:     return "VCD";
            case DemosaicAlgorithm::mixed:   return "Mixed";
            case DemosaicAlgorithm::lmmse:   return "LMMSE";
            case DemosaicAlgorithm::amaze:   return "AMaZE";
            case DemosaicAlgorithm::dht:     return "DHT";
            case DemosaicAlgorithm::aahd:    return "AAHD";
            case DemosaicAlgorithm::none:    return "none";
            default:                         return "AHD";
        }
    }

    /**
     * @brief Checks if half-size output is enabled.
     * @return true if output will be half resolution (faster loading).
     */
    [[nodiscard]] constexpr bool get_half_size() const noexcept { return m_half_size; }

    /**
     * @brief Enables or disables half-size output mode.
     * @param half_size true for half resolution, false for full resolution.
     */
    constexpr void set_half_size(bool half_size) noexcept { m_half_size = half_size; }
    // ═════════════════════════════════════════════════════════════════════════
    // COLOR SPACE SETTINGS
    // ═════════════════════════════════════════════════════════════════════════

private:
    /** @brief The color space to use for RAW output.
     */
    RawColorSpace m_color_space{RawColorSpace::prophoto_linear};

public:
    /**
     * @brief Gets the output color space.
     * @return The current color space.
     */
    [[nodiscard]] constexpr RawColorSpace get_color_space() const noexcept { return m_color_space; }

    /**
     * @brief Sets the output color space.
     * @param color_space The color space to use for output.
     */
    constexpr void set_color_space(RawColorSpace color_space) noexcept { m_color_space = color_space; }

    /**
     * @brief Gets the color space name as string (for OIIO attribute).
     * @return The color space name string (e.g., "ProPhoto-linear").
     */
    [[nodiscard]] std::string get_color_space_string() const noexcept {
        switch (m_color_space) {
            case RawColorSpace::raw:             return "raw";
            case RawColorSpace::srgb:            return "sRGB";
            case RawColorSpace::srgb_linear:     return "sRGB-linear";
            case RawColorSpace::adobe:           return "Adobe";
            case RawColorSpace::wide:            return "Wide";
            case RawColorSpace::prophoto:        return "ProPhoto";
            case RawColorSpace::prophoto_linear: return "ProPhoto-linear";
            case RawColorSpace::xyz:             return "XYZ";
            case RawColorSpace::aces:            return "ACES";
            case RawColorSpace::dci_p3:          return "DCI-P3";
            case RawColorSpace::rec2020:         return "Rec2020";
            default:                             return "ProPhoto-linear";
        }
    }
    // ═════════════════════════════════════════════════════════════════════════
    // HIGHLIGHT RECOVERY SETTINGS
    // ═════════════════════════════════════════════════════════════════════════

private:
    /** @brief The highlight recovery mode to use.
     */
    HighlightMode m_highlight_mode{HighlightMode::blend};

    /** @brief Flag indicating whether to clamp the highlight balance.
     * When true, this resolves color shifts in clipped highlights caused by
     * unequal channel saturation. Note: This changes output datatype to HALF.
     */
    bool m_balance_clamped{true};

public:
    /**
     * @brief Gets the highlight recovery mode.
     * @return The current highlight mode.
     */
    [[nodiscard]] constexpr HighlightMode get_highlight_mode() const noexcept { return m_highlight_mode; }

    /**
     * @brief Sets the highlight recovery mode.
     * @param mode The highlight mode to use.
     */
    constexpr void set_highlight_mode(HighlightMode mode) noexcept { m_highlight_mode = mode; }

    /**
     * @brief Gets the highlight mode value as integer (for OIIO attribute).
     * @return The highlight mode integer value.
     */
    [[nodiscard]] constexpr int get_highlight_mode_value() const noexcept { 
        return static_cast<int>(m_highlight_mode); 
    }

    /**
     * @brief Checks if highlight clamping balance is enabled.
     * @return true if balance_clamped is enabled.
     * When enabled, this resolves color shifts in clipped highlights caused by
     * unequal channel saturation. Note: This changes output datatype to HALF.
     */
    [[nodiscard]] constexpr bool get_balance_clamped() const noexcept { return m_balance_clamped; }

    /**
     * @brief Enables or disables highlight clamping balance.
     * @param balance true to enable, false to disable.
     */
    constexpr void set_balance_clamped(bool balance) noexcept { m_balance_clamped = balance; }
    // ═════════════════════════════════════════════════════════════════════════
    // WHITE BALANCE SETTINGS
    // ═════════════════════════════════════════════════════════════════════════

private:
    /**
     * @brief Use camera white balance.
     * OIIO precedence rules:
     * 1. If use_camera_wb == 1 → use camera WB (ignores auto_wb, greybox, user_mul)
     * 2. If use_camera_wb == 0 && use_auto_wb == 1 → auto WB from whole image
     * 3. If use_camera_wb == 0 && use_auto_wb == 0 && greybox valid → greybox WB
     * 4. If use_camera_wb == 0 && use_auto_wb == 0 && greybox empty → user_mul
     */
    bool m_use_camera_wb{true};
    /** @brief Flag indicating whether to use auto white balance.
     * When true, the white balance will be automatically calculated from the image.
     */
    bool m_use_auto_wb{false};
    std::array<int, 4> m_grey_box{0, 0, 0, 0};           ///< X, Y, Width, Height for grey card WB
    std::array<float, 4> m_user_mul{1.0f, 1.0f, 1.0f, 1.0f}; ///< R, G, B, G2 multipliers

public:

    /**
     * @brief Checks if camera white balance is enabled.
     * @return true if camera WB will be used.
     */
    [[nodiscard]] constexpr bool get_use_camera_wb() const noexcept { return m_use_camera_wb; }

    /**
     * @brief Enables or disables camera white balance.
     * @param use_camera true to use camera WB, false to use alternative method.
     */
    constexpr void set_use_camera_wb(bool use_camera) noexcept { m_use_camera_wb = use_camera; }

    /**
     * @brief Checks if auto white balance is enabled.
     * @return true if auto WB will be used (only if use_camera_wb is false).
     */
    [[nodiscard]] constexpr bool get_use_auto_wb() const noexcept { return m_use_auto_wb; }

    /**
     * @brief Enables or disables auto white balance.
     * @param use_auto true to enable auto WB calculation.
     * @note Only applies if use_camera_wb is false.
     */
    constexpr void set_use_auto_wb(bool use_auto) noexcept { m_use_auto_wb = use_auto; }

    /**
     * @brief Gets the white balance mode as high-level enum.
     * @return The current white balance mode.
     */
    [[nodiscard]] constexpr WhiteBalanceMode get_white_balance_mode() const noexcept {
        if (m_use_camera_wb) {
            return WhiteBalanceMode::camera;
        }
        if (m_use_auto_wb) {
            return WhiteBalanceMode::auto_wb;
        }
        // Check if grey_box is valid (non-zero size)
        if (m_grey_box[2] > 0 && m_grey_box[3] > 0) {
            return WhiteBalanceMode::grey_box;
        }
        return WhiteBalanceMode::user_mul;
    }

    /**
     * @brief Sets the white balance mode from high-level enum.
     * @param mode The white balance mode to use.
     */
    constexpr void set_white_balance_mode(WhiteBalanceMode mode) noexcept {
        switch (mode) {
            case WhiteBalanceMode::none:
                m_use_camera_wb = false;
                m_use_auto_wb = false;
                break;
            case WhiteBalanceMode::camera:
                m_use_camera_wb = true;
                m_use_auto_wb = false;
                break;
            case WhiteBalanceMode::auto_wb:
                m_use_camera_wb = false;
                m_use_auto_wb = true;
                break;
            case WhiteBalanceMode::grey_box:
                m_use_camera_wb = false;
                m_use_auto_wb = false;
                break;
            case WhiteBalanceMode::user_mul:
                m_use_camera_wb = false;
                m_use_auto_wb = false;
                m_grey_box = {0, 0, 0, 0};  // Disable grey_box to use user_mul
                break;
        }
    }
    /**
     * @brief Gets the grey box region for white balance calculation.
     * @return Array containing {X, Y, Width, Height}. Zero size means disabled.
     */
    [[nodiscard]] constexpr const std::array<int, 4>& get_grey_box() const noexcept { return m_grey_box; }

    /**
     * @brief Sets the grey box region for white balance calculation.
     * @param x X coordinate of top-left corner.
     * @param y Y coordinate of top-left corner.
     * @param width Width of the region.
     * @param height Height of the region.
     * @note Automatically disables use_camera_wb and use_auto_wb.
     */
    constexpr void set_grey_box(int x, int y, int width, int height) noexcept {
        m_grey_box = {x, y, width, height};
        // Disable other WB modes to use grey_box
        m_use_camera_wb = false;
        m_use_auto_wb = false;
    }

    /**
     * @brief Gets the user-defined white balance multipliers.
     * @return Array containing {R, G, B, G2} multipliers.
     */
    [[nodiscard]] constexpr const std::array<float, 4>& get_user_mul() const noexcept { return m_user_mul; }

    /**
     * @brief Sets the user-defined white balance multipliers.
     * @param r Red channel multiplier.
     * @param g Green channel multiplier.
     * @param b Blue channel multiplier.
     * @param g2 Second green channel multiplier (for Bayer RGGB patterns).
     * @note Automatically disables use_camera_wb, use_auto_wb, and clears grey_box.
     */
    constexpr void set_user_mul(float r, float g, float b, float g2) noexcept {
        m_user_mul = {r, g, b, g2};
        // Disable other WB modes to use user_mul
        m_use_camera_wb = false;
        m_use_auto_wb = false;
        m_grey_box = {0, 0, 0, 0};
    }
    // ═════════════════════════════════════════════════════════════════════════
    // COLOR MATRIX SETTINGS
    // ═════════════════════════════════════════════════════════════════════════

private:
    /** @brief The camera color matrix usage mode. */
    CameraMatrixMode m_camera_matrix{CameraMatrixMode::always};

public:
    /**
     * @brief Gets the camera color matrix usage mode.
     * @return The current camera matrix mode.
     */
    [[nodiscard]] constexpr CameraMatrixMode get_camera_matrix() const noexcept { return m_camera_matrix; }

    /**
     * @brief Sets the camera color matrix usage mode.
     * @param mode The camera matrix mode to use.
     */
     constexpr void set_camera_matrix(CameraMatrixMode mode) noexcept { m_camera_matrix = mode; }

     /**
     * @brief Gets the camera matrix mode value as integer (for OIIO attribute).
     * @return The camera matrix mode integer value.
     */
    [[nodiscard]] constexpr int get_camera_matrix_value() const noexcept { 
        return static_cast<int>(m_camera_matrix); 
    }
    // ═════════════════════════════════════════════════════════════════════════
    // EXPOSURE SETTINGS
    // ═════════════════════════════════════════════════════════════════════════
private:
    float m_exposure{1.0f};             ///< Exposure multiplier (0.25 to 8.0)
    bool m_auto_bright{false};           ///< Auto brightness adjustment
    bool m_apply_scene_linear_scale{false};///< Apply scene-linear scaling
    float m_camera_to_scene_linear_scale{2.2222222222222223f}; ///< Scale factor for scene-linear

public:
    /**
     * @brief Gets the exposure multiplier.
     * @return The exposure multiplier (1.0 = no correction).
     * Range: 0.25 (2 stops darken) to 8.0 (3 stops brighten).
     */
    [[nodiscard]] constexpr float get_exposure() const noexcept { return m_exposure; }

    /**
     * @brief Sets the exposure multiplier.
     * @param exposure The exposure multiplier (0.25 to 8.0).
     */
    constexpr void set_exposure(float exposure) noexcept { m_exposure = exposure; }

    /**
     * @brief Checks if auto brightness is enabled.
     * @return true if auto brightness adjustment is enabled.
     */
    [[nodiscard]] constexpr bool get_auto_bright() const noexcept { return m_auto_bright; }

    /**
     * @brief Enables or disables auto brightness.
     * @param auto_bright true to enable auto brightness.
     */
    constexpr void set_auto_bright(bool auto_bright) noexcept { m_auto_bright = auto_bright; }

    /**
     * @brief Checks if scene-linear scaling is applied.
     * @return true if scene-linear scaling is enabled.
     * When enabled, applies scaling so 18% grey card has value 0.18.
     */
    [[nodiscard]] constexpr bool get_apply_scene_linear_scale() const noexcept { return m_apply_scene_linear_scale; }

    /**
     * @brief Enables or disables scene-linear scaling.
     * @param apply true to enable scene-linear scaling.
     */
    constexpr void set_apply_scene_linear_scale(bool apply) noexcept { m_apply_scene_linear_scale = apply; }

    /**
     * @brief Gets the camera-to-scene linear scale factor.
     * @return The scale factor (default: 1.0/0.45 ≈ 2.222).
     */
    [[nodiscard]] constexpr float get_camera_to_scene_linear_scale() const noexcept { 
        return m_camera_to_scene_linear_scale; 
    }

    /**
     * @brief Sets the camera-to-scene linear scale factor.
     * @param scale The scale factor to use.
     */
    constexpr void set_camera_to_scene_linear_scale(float scale) noexcept { 
        m_camera_to_scene_linear_scale = scale; 
    }
    // ═════════════════════════════════════════════════════════════════════════
    // BLACK/WHITE POINT SETTINGS
    // ═════════════════════════════════════════════════════════════════════════

private:
    int m_user_black{-1};    ///< User-specified black point (<0 = auto)
    int m_user_sat{0};       ///< User-specified saturation point (0 = auto)
    float m_adjust_maximum_thr{0.0f}; ///< Auto-adjust maximum threshold

public:
    /**
     * @brief Gets the user-specified black point.
     * @return The black point value (<0 means auto).
     */
    [[nodiscard]] constexpr int get_user_black() const noexcept { return m_user_black; }

    /**
     * @brief Sets the user-specified black point.
     * @param black The black point value. Use negative for auto.
     */
    constexpr void set_user_black(int black) noexcept { m_user_black = black; }

    /**
     * @brief Gets the user-specified saturation point.
     * @return The saturation point value (0 means auto).
     */
    [[nodiscard]] constexpr int get_user_sat() const noexcept { return m_user_sat; }

    /**
     * @brief Sets the user-specified saturation point.
     * @param sat The saturation point value. Use 0 for auto.
     */
    constexpr void set_user_sat(int sat) noexcept { m_user_sat = sat; }

    /**
     * @brief Gets the auto-adjust maximum threshold.
     * @return The threshold value (0 = disabled).
     */
    [[nodiscard]] constexpr float get_adjust_maximum_thr() const noexcept { return m_adjust_maximum_thr; }

    /**
     * @brief Sets the auto-adjust maximum threshold.
     * @param thr The threshold value. Use 0 to disable.
     */
    constexpr void set_adjust_maximum_thr(float thr) noexcept { m_adjust_maximum_thr = thr; }
    // ═════════════════════════════════════════════════════════════════════════
    // NOISE REDUCTION SETTINGS
    // ═════════════════════════════════════════════════════════════════════════

private:
    /** @brief The FBDD noise reduction level. */
    FbddNoiseRd m_fbdd_noiserd{FbddNoiseRd::light};
    /** @brief The wavelet denoising threshold. */
    float m_threshold{0.0f};    ///< Wavelet denoising threshold (0-1000+)

public:
    /**
     * @brief Gets the FBDD noise reduction level.
     * @return The current FBDD noise reduction setting.
     */
    [[nodiscard]] constexpr FbddNoiseRd get_fbdd_noiserd() const noexcept { return m_fbdd_noiserd; }

    /**
     * @brief Sets the FBDD noise reduction level.
     * @param noiserd The FBDD noise reduction level.
     */
    constexpr void set_fbdd_noiserd(FbddNoiseRd noiserd) noexcept { m_fbdd_noiserd = noiserd; }

    /**
     * @brief Gets the FBDD noise reduction value as integer (for OIIO attribute).
     * @return The FBDD noise reduction integer value.
     */
    [[nodiscard]] constexpr int get_fbdd_noiserd_value() const noexcept { 
        return static_cast<int>(m_fbdd_noiserd); 
    }

    /**
     * @brief Gets the wavelet denoising threshold.
     * @return The threshold value (0 = disabled).
     * Sensible values are between 100 and 1000.
     */
    [[nodiscard]] constexpr float get_threshold() const noexcept { return m_threshold; }

    /**
     * @brief Sets the wavelet denoising threshold.
     * @param threshold The threshold value. Use 0 to disable.
     */
    constexpr void set_threshold(float threshold) noexcept { m_threshold = threshold; }
    // ═════════════════════════════════════════════════════════════════════════
    // CHROMATIC ABERRATION CORRECTION
    // ═════════════════════════════════════════════════════════════════════════
private:
    std::array<float, 2> m_aber{1.0f, 1.0f}; ///< Red and blue scale factors

public:

    /**
     * @brief Gets the chromatic aberration correction factors.
     * @return Array containing {red_scale, blue_scale}.
     * Default (1.0, 1.0) means no correction.
     * Sensible values are very close to 1.0.
     */
    [[nodiscard]] constexpr const std::array<float, 2>& get_aber() const noexcept { return m_aber; }

    /**
     * @brief Sets the chromatic aberration correction factors.
     * @param red Red channel scale factor.
     * @param blue Blue channel scale factor.
     */
    constexpr void set_aber(float red, float blue) noexcept { m_aber = {red, blue}; }
    // ═════════════════════════════════════════════════════════════════════════
    // ORIENTATION SETTINGS
    // ═════════════════════════════════════════════════════════════════════════

private:
    /** @brief The user-specified orientation. */
    Orientation m_user_flip{Orientation::ignored};

public:
    /**
     * @brief Gets the user-specified orientation.
     * @return The orientation value.
     */
    [[nodiscard]] constexpr Orientation get_user_flip() const noexcept { return m_user_flip; }

    /**
     * @brief Sets the user-specified orientation.
     * @param flip The orientation to use.
     */
    constexpr void set_user_flip(Orientation flip) noexcept { m_user_flip = flip; }

    /**
     * @brief Gets the orientation value as integer (for OIIO attribute).
     * @return The orientation integer value.
     */
    [[nodiscard]] constexpr int get_user_flip_value() const noexcept { 
        return static_cast<int>(m_user_flip); 
    }
    // ═════════════════════════════════════════════════════════════════════════
    // CROP SETTINGS
    // ═════════════════════════════════════════════════════════════════════════

private:
    std::array<int, 4> m_crop_box{0, 0, 0, 0}; ///< X, Y, Width, Height

public:
    /**
     * @brief Gets the crop box region.
     * @return Array containing {X, Y, Width, Height}. Zero size means auto-crop.
     */
    [[nodiscard]] constexpr const std::array<int, 4>& get_crop_box() const noexcept { return m_crop_box; }

    /**
     * @brief Sets the crop box region.
     * @param x X coordinate of top-left corner.
     * @param y Y coordinate of top-left corner.
     * @param width Width of the crop region.
     * @param height Height of the crop region.
     */
    constexpr void set_crop_box(int x, int y, int width, int height) noexcept {
        m_crop_box = {x, y, width, height};
    }
    // ═════════════════════════════════════════════════════════════════════════
    // MEMORY AND LOADING SETTINGS
    // ═════════════════════════════════════════════════════════════════════════

private:
    int m_max_raw_memory_mb{4096}; ///< Maximum memory for RAW processing (MB)
    bool m_force_load{true};     ///< Force decompress during initialization

public:

    /**
     * @brief Gets the maximum memory allocation for RAW processing.
     * @return The maximum memory in megabytes.
     */
    [[nodiscard]] constexpr int get_max_raw_memory_mb() const noexcept { return m_max_raw_memory_mb; }

    /**
     * @brief Sets the maximum memory allocation for RAW processing.
     * @param max_mb The maximum memory in megabytes.
     */
    constexpr void set_max_raw_memory_mb(int max_mb) noexcept { m_max_raw_memory_mb = max_mb; }

    /**
     * @brief Checks if force load is enabled.
     * @return true if decompression is forced during initialization.
     */
    [[nodiscard]] constexpr bool get_force_load() const noexcept { return m_force_load; }

    /**
     * @brief Enables or disables force load.
     * @param force true to force decompression during initialization.
     */
    constexpr void set_force_load(bool force) noexcept { m_force_load = force; }
    // ═════════════════════════════════════════════════════════════════════════
    // PRESET FACTORY METHODS
    // ═════════════════════════════════════════════════════════════════════════

    /**
     * @brief Creates a RawSettings configured for maximum processing speed.
     * @return RawSettings with speed-optimized configuration.
     * ## Configuration Rationale
     * | Parameter | Value | Reason |
     * |-----------|-------|--------|
     * | demosaic | linear | Fastest algorithm |
     * | color_space | srgb_linear | Minimal color conversion |
     * | highlight_mode | clip | No reconstruction overhead |
     * | half_size | true | Half resolution = 1/4 pixels |
     * | fbdd_noiserd | off | No pre-demosaic denoising |
     * | force_load | false | Lazy loading |
     * | balance_clamped | false | No additional processing |
     * @note Use for preview/thumbnail generation or when speed is critical.
     * @warning Quality will be significantly reduced compared to other presets.
     */
    [[nodiscard]] static constexpr RawSettings fast_settings() noexcept {
        RawSettings settings;
        settings.m_demosaic = DemosaicAlgorithm::linear;
        settings.m_color_space = RawColorSpace::srgb_linear;
        settings.m_highlight_mode = HighlightMode::clip;
        settings.m_balance_clamped = false;
        settings.m_half_size = true;
        settings.m_fbdd_noiserd = FbddNoiseRd::off;
        settings.m_force_load = false;
        settings.m_camera_matrix = CameraMatrixMode::dng_only;
        settings.m_use_camera_wb = true;
        settings.m_use_auto_wb = false;
        return settings;
    }

    /**
     * @brief Creates a RawSettings configured for maximum image quality.
     * @return RawSettings with quality-optimized configuration.
     * ## Configuration Rationale
     * | Parameter | Value | Reason |
     * |-----------|-------|--------|
     * | demosaic | amaze | Best edge handling, minimal artifacts |
     * | color_space | prophoto_linear | Widest gamut for editing |
     * | highlight_mode | rebuild | Best recovery for clipped highlights |
     * | half_size | false | Full resolution |
     * | fbdd_noiserd | light | Improves amaze accuracy |
     * | force_load | true | All metadata available |
     * | balance_clamped | true | Prevents highlight color shifts |
     * | use_camera_matrix | always | Accurate color reproduction |
     * @note Use for final export or when quality is paramount.
     * @warning Processing will be significantly slower than other presets.
     */
    [[nodiscard]] static constexpr RawSettings quality_settings() noexcept {
        RawSettings settings;
        settings.m_demosaic = DemosaicAlgorithm::amaze;
        settings.m_color_space = RawColorSpace::prophoto_linear;
        settings.m_highlight_mode = HighlightMode::rebuild;
        settings.m_balance_clamped = true;
        settings.m_half_size = false;
        settings.m_fbdd_noiserd = FbddNoiseRd::light;
        settings.m_force_load = true;
        settings.m_camera_matrix = CameraMatrixMode::always;
        settings.m_use_camera_wb = true;
        settings.m_use_auto_wb = false;
        return settings;
    }

    /**
     * @brief Creates a RawSettings configured for balanced performance and quality.
     * @return RawSettings with balanced configuration.
     * ## Configuration Rationale
     * | Parameter | Value | Reason |
     * |-----------|-------|--------|
     * | demosaic | ahd | Good quality/speed balance |
     * | color_space | prophoto_linear | Wide gamut for editing |
     * | highlight_mode | blend | Natural highlight recovery |
     * | half_size | false | Full resolution |
     * | fbdd_noiserd | light | Moderate noise reduction |
     * | force_load | true | Metadata available |
     * | balance_clamped | true | Prevents highlight color shifts |
     * | use_camera_matrix | always | Accurate color reproduction |
     * @note This is the recommended default for interactive editing sessions.
     * @note Provides ~2-3x faster processing than quality_settings() with good quality.
     */
    [[nodiscard]] static constexpr RawSettings performance_settings() noexcept {
        RawSettings settings;
        settings.m_demosaic = DemosaicAlgorithm::ahd;
        settings.m_color_space = RawColorSpace::prophoto_linear;
        settings.m_highlight_mode = HighlightMode::blend;
        settings.m_balance_clamped = true;
        settings.m_half_size = false;
        settings.m_fbdd_noiserd = FbddNoiseRd::light;
        settings.m_force_load = true;
        settings.m_camera_matrix = CameraMatrixMode::always;
        settings.m_use_camera_wb = true;
        settings.m_use_auto_wb = false;
        return settings;
    }
    // ═════════════════════════════════════════════════════════════════════════
    // UTILITY METHODS
    // ═════════════════════════════════════════════════════════════════════════

    /**
     * @brief Resets all settings to their default values.
     * Default values are optimized for quality (same as quality_settings()).
     */
    void reset_to_defaults() noexcept {
        *this = RawSettings{};
    }

    /**
     * @brief Checks if the current configuration matches quality preset.
     * @return true if configuration uses quality-optimized settings.
     */
    [[nodiscard]] constexpr bool is_quality_configuration() const noexcept {
        return m_demosaic == DemosaicAlgorithm::amaze &&
               m_color_space == RawColorSpace::prophoto_linear &&
               m_highlight_mode >= HighlightMode::blend &&
               m_balance_clamped &&
               m_camera_matrix == CameraMatrixMode::always;
    }

    /**
     * @brief Checks if the current configuration matches fast preset.
     * @return true if configuration uses speed-optimized settings.
     */
    [[nodiscard]] constexpr bool is_fast_configuration() const noexcept {
        return m_half_size &&
               m_demosaic == DemosaicAlgorithm::linear;
    }

    /**
     * @brief Checks if the current configuration matches performance preset.
     * @return true if configuration uses balanced performance settings.
     */
    [[nodiscard]] constexpr bool is_performance_configuration() const noexcept {
        return m_demosaic == DemosaicAlgorithm::ahd &&
               m_color_space == RawColorSpace::prophoto_linear &&
               m_highlight_mode == HighlightMode::blend &&
               m_balance_clamped &&
               !m_half_size;
    }
};
} // namespace Raw
} // namespace CaptureMoment::Core::ImageConfig
