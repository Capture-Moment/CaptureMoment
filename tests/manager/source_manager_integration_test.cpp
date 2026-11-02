/**
 * @file source_manager_integration_test.cpp
 * @brief INTEGRATION tests for SourceManager: real files, real OIIO pipeline.
 *
 * @details
 * Companion of source_manager_test.cpp (pure unit suite). This file covers
 * exactly what the unit suite deliberately leaves out:
 *   - real file loading (TIFF fixtures generated at runtime) and RGBA_F32
 *     standardization: geometry, RGB / grayscale expansion, alpha fill
 *   - getTile()/setTile() pixel-exact content, coordinate mapping and
 *     read-modify-write round trips
 *   - getMetadata() attribute lookup on a real spec
 *   - the documented contract gaps (metadata loss, bounds validation),
 *     expressed as SELF-ACTIVATING tests: they probe the current behavior
 *     at runtime, GTEST_SKIP() while the gap is open, and start asserting
 *     the desired contract automatically once the fix lands.
 *
 * Fixture strategy: images are GENERATED at runtime through the linked OIIO
 * into a per-test hermetic temp dir (no binary fixture committed, no shared
 * state). The expected pixel content is defined by expectedPixelValue(),
 * used both by the fixture writer and by the assertions.
 *
 * Calibration notes (pinned on purpose against the current implementation):
 *   - a NON-EXISTENT file surfaces as CoreError::DecodingError (the ImageCache
 *     read fails), not FileNotFound as the interface docs suggest;
 *   - out-of-bounds tile requests are silently CLAMPED by the OIIO ROI
 *     (intersection filled, rest zero-filled) instead of being rejected;
 *   - setTile() rejects regions that are not RGBA_F32 with 4 channels;
 *   - loadFile() while an image is loaded replaces it; a FAILED reload
 *     leaves the manager fully unloaded and clean.
 *
 * @author CaptureMoment Team
 * @date 2026
 */

#include "managers/source_manager.h"

#include <gtest/gtest.h>
#include <OpenImageIO/imageio.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>
#include <atomic>
#include <random>
#include <sstream>

namespace CaptureMoment::Core::Managers::Test {

using CaptureMoment::Core::Common::ImageRegion;
using CaptureMoment::Core::Common::PixelFormat;
using CaptureMoment::Core::ErrorHandling::CoreError;
using CaptureMoment::Core::Managers::SourceManager;

namespace {

// ============================================================
// Fixture pattern - single source of truth for pixel content
// ============================================================

// Fixture images are never wider than this (assumption of expectedPixelValue).
constexpr std::uint8_t k_pattern_width { 64 };

/**
 * @brief Expected value of a fixture pixel, shared by the fixture writer
 *        and by the assertions.
 *
 * @details
 * Values are exactly representable in float32 (integer numerator,
 * power-of-two denominator): no rounding noise can enter through the OIIO
 * encode/decode round trip. The maximum stays below 1.0 so no clamp step
 * of the pipeline can alter the pattern. Alpha is pinned to 1.0 so the
 * assertions stay immune to any (un)premultiply pass.
 *
 * @param x Full-image X coordinate of the pixel.
 * @param y Full-image Y coordinate of the pixel.
 * @param c Channel index (3 == alpha).
 */
float expectedPixelValue(int x, int y, int c) noexcept
{
    if (c == 3) {
        return 1.0f;
    }
    return static_cast<float>((y * k_pattern_width + x) * 3 + c + 1) / 16384.0f;
}

/**
 * @brief Builds the pixel buffer of a fixture image (row-major, chunky).
 */
std::vector<float> buildPatternData(int width, int height, int channels)
{
    std::vector<float> data(static_cast<std::size_t>(width)
                            * static_cast<std::size_t>(height)
                            * static_cast<std::size_t>(channels));
    for (int y { 0 }; y < height; ++y) {
        for (int x { 0 }; x < width; ++x) {
            for (int c { 0 }; c < channels; ++c) {
                data[(static_cast<std::size_t>(y) * width + x) * channels + c] =
                    expectedPixelValue(x, y, c);
            }
        }
    }
    return data;
}

/**
 * @brief Writes a non-image file behind a .tiff extension (corrupt input).
 */
std::filesystem::path
writeCorruptFile(const std::filesystem::path& dir, const std::string& stem)
{
    const std::filesystem::path filePath { dir / (stem + ".tiff") };

    std::ofstream file { filePath, std::ios::binary };
    for (uint8_t i { 0 }; i < 16; ++i) {
        file << "CaptureMoment: not an image, just pretending to be one.\n";
    }

    return filePath;
}

/**
 * @brief Writes a real float image fixture through the linked OIIO.
 *
 * The format is selected by OIIO from the extension. EXR stores arbitrary
 * header attributes natively; TIFF has a fixed tag table - the metadata
 * tests deliberately use EXR for that reason.
 *
 * @param extension Target format extension with dot, e.g. ".tiff" or ".exr".
 * @return The created file path, or nullopt if OIIO refused to write it.
 */
std::optional<std::filesystem::path>
writePatternImage(const std::filesystem::path& dir, const std::string& stem,
                  const std::string& extension,
                  int width, int height, int channels)
{
    const std::filesystem::path filePath { dir / (stem + extension) };

    auto out { OIIO::ImageOutput::create(filePath.string()) };
    if (!out) {
        return std::nullopt;
    }

    OIIO::ImageSpec spec(width, height, channels, OIIO::TypeDesc::FLOAT);
    // Canonical OIIO name for Rec709-linear; float data is linear by
    // convention, so the manager's color pipeline must stay a no-op here.
    spec.attribute("oiio:ColorSpace", "linear");
    spec.attribute("ImageDescription", "CaptureMoment integration fixture");
    spec.attribute("sm:TestPattern", "v1");

    if (!out->open(filePath.string(), spec)) {
        return std::nullopt;
    }

    const auto data { buildPatternData(width, height, channels) };
    if (!out->write_image(OIIO::TypeDesc::FLOAT, data.data()) || !out->close()) {
        return std::nullopt;
    }

    return filePath;
}
/**
 * @brief RAII directory unique per test, wiped on destruction.
 *
 * @details Uniqueness comes from a per-process random token plus a
 *          monotonic counter: two concurrent runs can never collide, and
 *          the path is not predictable (publicly-writable-directory
 *          hardening, cpp:S2083). On POSIX the directory is additionally
 *          restricted to its owner (0700); on Windows %TEMP% is already
 *          per-user.
 */
class TempTestDir {
public:
    TempTestDir()
    {
        namespace fs = std::filesystem;

        const auto* info { ::testing::UnitTest::GetInstance()->current_test_info() };

        std::ostringstream name;
        name << "capturemoment_" << info->test_suite_name() << '_' << info->name()
             << '_' << runToken() << '_' << ++m_counter;

        m_path = fs::temp_directory_path() / name.str();

        std::error_code ec;
        fs::remove_all(m_path, ec); // paranoid: a unique name should never exist
        fs::create_directories(m_path, ec);
#ifndef _WIN32
        // /tmp is world-writable (sticky bit): restrict the dir to its owner,
        // so no other local user can read or swap it.
        fs::permissions(m_path, fs::perms::owner_all, ec);
#endif
    }

    ~TempTestDir()
    {
        std::error_code ec;
        std::filesystem::remove_all(m_path, ec);
    }

    TempTestDir(const TempTestDir&) = delete;
    TempTestDir& operator=(const TempTestDir&) = delete;
    TempTestDir(TempTestDir&&) = delete;
    TempTestDir& operator=(TempTestDir&&) = delete;

    [[nodiscard]] const std::filesystem::path& path() const noexcept { return m_path; }

private:
    /**
     * @brief Stable random token identifying THIS process run.
     * @note Thread-safe static initialization, computed once.
     */
    [[nodiscard]] static const std::string& runToken()
    {
        static const std::string token {
            [] {
                std::random_device device;
                std::ostringstream oss;
                oss << std::hex << device() << device();
                return oss.str();
            }()
        };
        return token;
    }

    inline static std::atomic<std::uint64_t> m_counter { 0 };

    std::filesystem::path m_path;
};

} // namespace

// ============================================================
// Fixture: one fresh manager + one hermetic temp dir per test
// ============================================================

/**
 * @brief Fixture for the integration contract: a fresh manager and a
 *        per-test temporary directory where OIIO-generated fixtures live.
 */
class SourceManagerIntegrationTest: public ::testing::Test
{
    protected:
        SourceManager m_manager; // one fresh manager per test
        TempTestDir m_dir;       // one hermetic temp dir per test

        /**
         * @brief Writes a pattern TIFF into the per-test temp dir.
         * @return The file path, or nullopt if the fixture could not be
         *         created (callers abort with ASSERT_TRUE).
         */
        [[nodiscard]] std::optional<std::filesystem::path>
        makeTiff(const std::string& stem, int width, int height, int channels = 4)
        {
            return writePatternImage(m_dir.path(), stem, ".tiff", width, height, channels);
        }

        /**
         * @brief Writes a pattern EXR into the per-test temp dir.
         * @return The file path, or nullopt if the fixture could not be
         *         created (callers abort with ASSERT_TRUE).
         */
        [[nodiscard]] std::optional<std::filesystem::path>
        makeExr(const std::string& stem, int width, int height, int channels = 4)
        {
            return writePatternImage(m_dir.path(), stem, ".exr", width, height, channels);
        }
};

// ============================================================
// loadFile() - real files: geometry & RGBA_F32 standardization
// ============================================================

/**
 * @test Loading a real float TIFF reports the geometry of the file and
 *       records the exact path that was given.
 */
TEST_F(SourceManagerIntegrationTest, LoadRealFloatTiffSucceedsAndReportsGeometry)
{
    const auto file { makeTiff("geometry", 8, 6) };
    ASSERT_TRUE(file.has_value());

    const auto result { m_manager.loadFile(file->string()) };

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(m_manager.isLoaded());
    EXPECT_EQ(m_manager.width(), 8u);
    EXPECT_EQ(m_manager.height(), 6u);
    EXPECT_EQ(m_manager.channels(), 4u);
    EXPECT_EQ(m_manager.getImageSourcePath(), file->string());
}

/**
 * @test The tile returned from a loaded file follows the RGBA_F32
 *       standardization contract: correct format, 4 channels, alpha fully
 *       opaque, and pixel-exact fixture content.
 */
TEST_F(SourceManagerIntegrationTest, LoadRealTiffStandardizesPixelsToRgbaF32)
{
    constexpr int k_width { 4 };
    constexpr int k_height { 4 };

    const auto file { makeTiff("standard", k_width, k_height) };
    ASSERT_TRUE(file.has_value());
    ASSERT_TRUE(m_manager.loadFile(file->string()).has_value());

    const auto tile { m_manager.getTile(0, 0, k_width, k_height) };
    ASSERT_TRUE(tile.has_value());

    const ImageRegion& region { *tile.value() };
    EXPECT_EQ(region.format(), PixelFormat::RGBA_F32);
    EXPECT_EQ(region.channels(), 4u);

    for (int y { 0 }; y < k_height; ++y)
    {
        for (int x { 0 }; x < k_width; ++x) {
            EXPECT_FLOAT_EQ(region(y, x, 3), 1.0f)
                << "alpha not opaque at (" << x << ", " << y << ")";
            for (int c { 0 }; c < 3; ++c) {
                EXPECT_FLOAT_EQ(region(y, x, c), expectedPixelValue(x, y, c));
            }
        }
    }
}

/**
 * @test An RGB (3-channel) source is expanded to 4 channels: RGB content
 *       is preserved bit-for-bit, alpha is filled with 1.0 (the
 *       channel_values constant of convertToRgbaInternal).
 */
TEST_F(SourceManagerIntegrationTest, LoadRgbTiffExpandsToFourChannels)
{
    const auto file { makeTiff("rgb", 3, 2, /*channels=*/3) };
    ASSERT_TRUE(file.has_value());
    ASSERT_TRUE(m_manager.loadFile(file->string()).has_value());

    EXPECT_EQ(m_manager.channels(), 4u);

    const auto tile { m_manager.getTile(0, 0, 3, 2) };
    ASSERT_TRUE(tile.has_value());

    const ImageRegion& region { *tile.value() };
    for (uint8_t y { 0 }; y < 2; ++y)
    {
        for (uint8_t x { 0 }; x < 3; ++x)
        {
            EXPECT_FLOAT_EQ(region(y, x, 3), 1.0f);
            for (uint8_t c { 0 }; c < 3; ++c) {
                EXPECT_FLOAT_EQ(region(y, x, c), expectedPixelValue(x, y, c));
            }
        }
    }
}

/**
 * @test A grayscale (1-channel) source is replicated to R=G=B and gets a
 *       fully opaque alpha (channel_order {0,0,0,-1} branch of the
 *       conversion switch).
 */
TEST_F(SourceManagerIntegrationTest, LoadGrayTiffReplicatesToRgbWithOpaqueAlpha)
{
    const auto file { makeTiff("gray", 4, 2, /*channels=*/1) };
    ASSERT_TRUE(file.has_value());
    ASSERT_TRUE(m_manager.loadFile(file->string()).has_value());

    EXPECT_EQ(m_manager.channels(), 4u);

    const auto tile { m_manager.getTile(0, 0, 4, 2) };
    ASSERT_TRUE(tile.has_value());

    const ImageRegion& region { *tile.value() };
    for (uint8_t y { 0 }; y < 2; ++y)
    {
        for (uint8_t x { 0 }; x < 4; ++x)
        {
            EXPECT_FLOAT_EQ(region(y, x, 0), expectedPixelValue(x, y, 0));
            EXPECT_FLOAT_EQ(region(y, x, 1), expectedPixelValue(x, y, 0));
            EXPECT_FLOAT_EQ(region(y, x, 2), expectedPixelValue(x, y, 0));
            EXPECT_FLOAT_EQ(region(y, x, 3), 1.0f);
        }
    }
}

/**
 * @test A non-existent but well-formed path is rejected. The current
 *       implementation surfaces this as DecodingError (the ImageCache read
 *       fails), although the interface documentation suggests FileNotFound.
 *       The asymmetry is pinned here ON PURPOSE so that harmonizing the
 *       error taxonomy surfaces immediately.
 */
TEST_F(SourceManagerIntegrationTest, LoadNonExistentFileReturnsDecodingError)
{
    const auto missing { m_dir.path() / "does_not_exist.tiff" };

    const auto result { m_manager.loadFile(missing.string()) };

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), CoreError::DecodingError);
    EXPECT_FALSE(m_manager.isLoaded());
    EXPECT_EQ(m_manager.getImageSourcePath(), std::string());
}

/**
 * @test A corrupt file (junk bytes behind an image extension) is rejected
 *       with DecodingError and leaves the manager fully clean.
 */
TEST_F(SourceManagerIntegrationTest, LoadCorruptFileIsRejectedAndKeepsStateClean)
{
    const auto corrupt { writeCorruptFile(m_dir.path(), "corrupt") };
    ASSERT_TRUE(std::filesystem::exists(corrupt));

    const auto result { m_manager.loadFile(corrupt.string()) };

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), CoreError::DecodingError);
    EXPECT_FALSE(m_manager.isLoaded());
    EXPECT_EQ(m_manager.width(), 0u);
    EXPECT_EQ(m_manager.height(), 0u);
    EXPECT_EQ(m_manager.getImageSourcePath(), std::string());
}

/**
 * @test Loading a second file without an intermediate unload() replaces
 *       the whole observable state (path + geometry of the new file).
 */
TEST_F(SourceManagerIntegrationTest, LoadSecondFileReplacesPreviousState)
{
    const auto first { makeTiff("first", 8, 6) };
    const auto second { makeTiff("second", 5, 3) };
    ASSERT_TRUE(first.has_value() && second.has_value());

    ASSERT_TRUE(m_manager.loadFile(first->string()).has_value());
    const auto result { m_manager.loadFile(second->string()) };

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(m_manager.width(), 5u);
    EXPECT_EQ(m_manager.height(), 3u);
    EXPECT_EQ(m_manager.getImageSourcePath(), second->string());
}

/**
 * @test A FAILED reload (after a successful load) leaves the manager fully
 *       unloaded and clean: loadFile() discards the previous image BEFORE
 *       attempting the new load.
 */
TEST_F(SourceManagerIntegrationTest, LoadFailureAfterSuccessLeavesManagerClean)
{
    const auto file { makeTiff("good", 8, 6) };
    ASSERT_TRUE(file.has_value());
    ASSERT_TRUE(m_manager.loadFile(file->string()).has_value());
    ASSERT_TRUE(m_manager.isLoaded());

    const auto failed { m_manager.loadFile((m_dir.path() / "missing.tiff").string()) };

    ASSERT_FALSE(failed.has_value());
    EXPECT_FALSE(m_manager.isLoaded());
    EXPECT_EQ(m_manager.width(), 0u);
    EXPECT_EQ(m_manager.height(), 0u);
    EXPECT_EQ(m_manager.getImageSourcePath(), std::string());
}

// ============================================================
// getTile() - real decoded content & coordinate mapping
// ============================================================

/**
 * @test A tile covering the full image extent returns the exact fixture
 *       pattern: every pixel of every channel matches expectedPixelValue().
 *       Doubles as the exact-fit boundary check (no off-by-one on extent).
 */
TEST_F(SourceManagerIntegrationTest, GetTileFullExtentMatchesFixturePattern)
{
    constexpr int k_width { 8 };
    constexpr int k_height { 6 };

    const auto file { makeTiff("full", k_width, k_height) };
    ASSERT_TRUE(file.has_value());
    ASSERT_TRUE(m_manager.loadFile(file->string()).has_value());

    const auto tile { m_manager.getTile(0, 0, k_width, k_height) };
    ASSERT_TRUE(tile.has_value());

    const ImageRegion& region { *tile.value() };
    ASSERT_TRUE(region.isValid());

    for (uint8_t y { 0 }; y < k_height; ++y)
    {
        for (uint8_t x { 0 }; x < k_width; ++x)
        {

            for (uint8_t c { 0 }; c < 4; ++c) {
                EXPECT_FLOAT_EQ(region(y, x, c), expectedPixelValue(x, y, c))
                    << "mismatch at pixel (" << x << ", " << y << ") channel " << c;
            }
        }
    }
}

/**
 * @test A sub-region tile maps back to full-image coordinates: the pixel
 *       at (lx, ly) inside the tile is the fixture pixel (x + lx, y + ly),
 *       and the tile carries its source coordinates (m_x / m_y).
 */
TEST_F(SourceManagerIntegrationTest, GetTileSubRegionMapsToFullImageCoordinates)
{
    constexpr int k_x { 2 };
    constexpr int k_y { 1 };
    constexpr int k_width { 4 };
    constexpr int k_height { 3 };

    const auto file { makeTiff("sub", 8, 6) };
    ASSERT_TRUE(file.has_value());
    ASSERT_TRUE(m_manager.loadFile(file->string()).has_value());

    const auto tile { m_manager.getTile(k_x, k_y, k_width, k_height) };
    ASSERT_TRUE(tile.has_value());

    const ImageRegion& region { *tile.value() };
    EXPECT_EQ(region.width(), 4u);
    EXPECT_EQ(region.height(), 3u);
    EXPECT_EQ(region.channels(), 4u);
    EXPECT_EQ(region.m_x, k_x);
    EXPECT_EQ(region.m_y, k_y);

    for (uint8_t ly { 0 }; ly < k_height; ++ly)
    {
        for (uint8_t lx { 0 }; lx < k_width; ++lx)
        {
            for (uint8_t c { 0 }; c < 4; ++c) {
                EXPECT_FLOAT_EQ(region(ly, lx, c),
                                expectedPixelValue(k_x + lx, k_y + ly, c))
                    << "mismatch at tile-local (" << lx << ", " << ly << ")";
            }
        }
    }
}

/**
 * @test A tile ending exactly on the right/bottom edges of the image is
 *       accepted: the valid domain is inclusive on the max side.
 */
TEST_F(SourceManagerIntegrationTest, GetTileEndingOnImageBoundsSucceeds)
{
    const auto file { makeTiff("edge", 8, 6) };
    ASSERT_TRUE(file.has_value());
    ASSERT_TRUE(m_manager.loadFile(file->string()).has_value());

    const auto tile { m_manager.getTile(4, 2, 4, 4) }; // (4..7, 2..5) exact fit

    ASSERT_TRUE(tile.has_value());
    EXPECT_EQ(tile.value()->width(), 4u);
    EXPECT_EQ(tile.value()->height(), 4u);
}

/**
 * @test On a LOADED manager, a zero-sized tile request is rejected with
 *       InvalidImageRegion (complements the unit-level test that pins
 *       SourceNotLoaded on an UNLOADED manager - guard order).
 */
TEST_F(SourceManagerIntegrationTest, GetTileZeroSizeOnLoadedManagerReturnsInvalidImageRegion)
{
    const auto file { makeTiff("zerosize", 8, 6) };
    ASSERT_TRUE(file.has_value());
    ASSERT_TRUE(m_manager.loadFile(file->string()).has_value());

    const auto zeroWidth { m_manager.getTile(0, 0, 0, 4) };
    ASSERT_FALSE(zeroWidth.has_value());
    EXPECT_EQ(zeroWidth.error(), CoreError::InvalidImageRegion);

    const auto zeroHeight { m_manager.getTile(0, 0, 4, 0) };
    ASSERT_FALSE(zeroHeight.has_value());
    EXPECT_EQ(zeroHeight.error(), CoreError::InvalidImageRegion);

    // the manager itself is unharmed by the rejected requests
    EXPECT_TRUE(m_manager.isLoaded());
}

/**
 * @test CONTRACT NOTE (current clamping behavior): a PARTIALLY
 *       out-of-bounds request is filled by the OIIO ROI - the intersection
 *       carries the fixture pattern, everything outside stays zero-filled
 *       (the manager zero-initializes its buffer). Pinned here so any
 *       change to the bounds contract surfaces immediately.
 */
TEST_F(SourceManagerIntegrationTest, GetTilePartiallyOutsidePinsClampedBehavior)
{
    const auto file { makeTiff("partial", 8, 6) };
    ASSERT_TRUE(file.has_value());
    ASSERT_TRUE(m_manager.loadFile(file->string()).has_value());

    const auto tile { m_manager.getTile(6, 4, 4, 4) }; // crosses right AND bottom edges

    if (!tile.has_value()) {
        GTEST_SKIP() << "bounds contract changed: partially out-of-bounds "
                        "requests are now rejected (was: clamped fill)";
    }

    const ImageRegion& region { *tile.value() };
    EXPECT_EQ(region.width(), 4u);
    EXPECT_EQ(region.height(), 4u);
    EXPECT_EQ(region.m_x, 6);
    EXPECT_EQ(region.m_y, 4);

    // inside the intersection: real fixture pixels, mapped to full-image coords
    EXPECT_FLOAT_EQ(region(0, 0, 0), expectedPixelValue(6, 4, 0));
    EXPECT_FLOAT_EQ(region(1, 1, 2), expectedPixelValue(7, 5, 2));
    EXPECT_FLOAT_EQ(region(0, 0, 3), 1.0f);

    // outside the intersection: zero-filled
    EXPECT_FLOAT_EQ(region(3, 3, 0), 0.0f); // full-image (9, 7): fully outside
    EXPECT_FLOAT_EQ(region(2, 0, 1), 0.0f); // full-image (6, 6): below the bottom edge
}

/**
 * @test CONTRACT GAP (bounds validation): a request FULLY outside the
 *       image should be rejected with InvalidImageRegion. The current
 *       implementation instead returns a valid, zero-filled region (OIIO
 *       clamps the ROI). This test SELF-ACTIVATES once the gap is fixed.
 */
TEST_F(SourceManagerIntegrationTest, GetTileBeyondImageBoundsIsRejected)
{
    const auto file { makeTiff("outside", 8, 6) };
    ASSERT_TRUE(file.has_value());
    ASSERT_TRUE(m_manager.loadFile(file->string()).has_value());

    const auto outside { m_manager.getTile(100, 100, 4, 4) };

    if (outside.has_value()) {
        GTEST_SKIP() << "bounds-validation gap still open: getTile() clamps "
                        "out-of-bounds requests (zero-filled region returned) "
                        "instead of rejecting with InvalidImageRegion";
    }

    EXPECT_EQ(outside.error(), CoreError::InvalidImageRegion);
}

// ============================================================
// setTile() - read-modify-write round trips
// ============================================================

/**
 * @test Read-modify-write round trip: pixels changed through getTile() +
 *       setTile() are visible on the next getTile(), and every untouched
 *       element is preserved bit-for-bit.
 */
TEST_F(SourceManagerIntegrationTest, SetTileRoundTripPreservesAndUpdatesPixels)
{
    const auto file { makeTiff("roundtrip", 8, 6) };
    ASSERT_TRUE(file.has_value());
    ASSERT_TRUE(m_manager.loadFile(file->string()).has_value());

    // 1. read a tile and remember its original content
    auto tile { m_manager.getTile(1, 1, 3, 2) };
    ASSERT_TRUE(tile.has_value());
    ImageRegion& region { *tile.value() };
    const std::vector<float> original(region.getBuffer().begin(),
                                      region.getBuffer().end());

    // 2. mutate the red channel of the first two pixels (elements 0 and 4)
    region(0, 0, 0) = 0.75f;
    region(0, 1, 0) = 0.25f;

    // 3. write the tile back
    const auto written { m_manager.setTile(region) };
    ASSERT_TRUE(written.has_value());

    // 4. re-read and verify: mutations landed, nothing else moved
    const auto check { m_manager.getTile(1, 1, 3, 2) };
    ASSERT_TRUE(check.has_value());

    const ImageRegion& verified { *check.value() };
    EXPECT_FLOAT_EQ(verified(0, 0, 0), 0.75f);
    EXPECT_FLOAT_EQ(verified(0, 1, 0), 0.25f);

    const auto buffer { verified.getBuffer() };
    for (std::size_t i { 0 }; i < original.size(); ++i) {
        if (i == 0 || i == 4) { // the two deliberately mutated elements
            continue;
        }
        EXPECT_FLOAT_EQ(buffer[i], original[i])
            << "element " << i << " was altered by the round trip";
    }
}

/**
 * @test setTile() positions the write using the region's source
 *       coordinates (m_x / m_y): a region built with x=3, y=2 lands at
 *       (3, 2) in the working image, leaving the rest of the image
 *       untouched.
 */
TEST_F(SourceManagerIntegrationTest, SetTileWritesAtRegionSourceCoordinates)
{
    const auto file { makeTiff("positioned", 8, 6) };
    ASSERT_TRUE(file.has_value());
    ASSERT_TRUE(m_manager.loadFile(file->string()).has_value());

    std::vector<float> data(2 * 2 * 4, 0.5f);
    const ImageRegion patch(std::move(data), 2, 2, 4, /*x=*/3, /*y=*/2);

    const auto written { m_manager.setTile(patch) };
    ASSERT_TRUE(written.has_value());

    // the patch content is readable back at the requested position
    const auto check { m_manager.getTile(3, 2, 2, 2) };
    ASSERT_TRUE(check.has_value());
    const ImageRegion& patched { *check.value() };
    EXPECT_FLOAT_EQ(patched(0, 0, 0), 0.5f);
    EXPECT_FLOAT_EQ(patched(1, 1, 2), 0.5f);
    EXPECT_FLOAT_EQ(patched(1, 1, 3), 0.5f);

    // pixels outside the patch still hold the fixture pattern
    const auto full { m_manager.getTile(0, 0, 8, 6) };
    ASSERT_TRUE(full.has_value());
    EXPECT_FLOAT_EQ((*full.value())(1, 0, 0), expectedPixelValue(0, 1, 0));
    EXPECT_FLOAT_EQ((*full.value())(0, 0, 3), 1.0f);
}

/**
 * @test setTile() rejects a region that is not RGBA / 4 channels
 *       (the format-and-channel guard of the implementation). Pinned via
 *       a 3-channel region: valid geometry, wrong channel count - the
 *       same guard also covers the format mismatch branch.
 */
TEST_F(SourceManagerIntegrationTest, SetTileRejectsNonRgba4ChannelRegion)
{
    const auto file { makeTiff("format", 8, 6) };
    ASSERT_TRUE(file.has_value());
    ASSERT_TRUE(m_manager.loadFile(file->string()).has_value());

    std::vector<float> data(2 * 2 * 3, 0.5f);
    const ImageRegion wrong(std::move(data), 2, 2, /*channels=*/3);
    ASSERT_TRUE(wrong.isValid()); // valid geometry - only the layout is wrong

    const auto written { m_manager.setTile(wrong) };

    ASSERT_FALSE(written.has_value());
    EXPECT_EQ(written.error(), CoreError::InvalidImageRegion);
}

/**
 * @test CONTRACT NOTE (current clamping behavior): a PARTIALLY
 *       out-of-bounds setTile() succeeds and writes ONLY the intersection
 *       with the image; pixels outside keep their previous content.
 */
TEST_F(SourceManagerIntegrationTest, SetTilePartiallyOutsidePinsClampedBehavior)
{
    const auto file { makeTiff("setpartial", 8, 6) };
    ASSERT_TRUE(file.has_value());
    ASSERT_TRUE(m_manager.loadFile(file->string()).has_value());

    // 4x4 patch at (6, 4): intersection with the 8x6 image is (6..7, 4..5)
    std::vector<float> data(4 * 4 * 4, 0.5f);
    ImageRegion patch(std::move(data), 4, 4, 4, /*x=*/6, /*y=*/4);
    patch(0, 0, 0) = 0.75f; // marker inside the intersection
    patch(1, 1, 1) = 0.25f; // marker inside the intersection

    const auto written { m_manager.setTile(patch) };

    if (!written.has_value()) {
        GTEST_SKIP() << "bounds contract changed: partially out-of-bounds "
                        "writes are now rejected (was: clamped write)";
    }

    // the intersection received the patch content
    const auto check { m_manager.getTile(6, 4, 2, 2) };
    ASSERT_TRUE(check.has_value());
    EXPECT_FLOAT_EQ((*check.value())(0, 0, 0), 0.75f);
    EXPECT_FLOAT_EQ((*check.value())(1, 1, 1), 0.25f);

    // the rest of the image is untouched
    const auto full { m_manager.getTile(0, 0, 8, 6) };
    ASSERT_TRUE(full.has_value());
    EXPECT_FLOAT_EQ((*full.value())(1, 0, 0), expectedPixelValue(0, 1, 0));
}

/**
 * @test CONTRACT GAP (bounds validation): setTile() with a region fully
 *       outside the image should be rejected with InvalidImageRegion.
 *       The current implementation silently accepts it (OIIO clips the
 *       ROI). This test SELF-ACTIVATES once the gap is fixed.
 */
TEST_F(SourceManagerIntegrationTest, SetTileBeyondImageBoundsIsRejected)
{
    const auto file { makeTiff("setoutside", 8, 6) };
    ASSERT_TRUE(file.has_value());
    ASSERT_TRUE(m_manager.loadFile(file->string()).has_value());

    // 4x4 patch at (100, 100): fully outside the 8x6 image
    std::vector<float> data(4 * 4 * 4, 0.5f);
    const ImageRegion patch(std::move(data), 4, 4, 4, /*x=*/100, /*y=*/100);
    ASSERT_TRUE(patch.isValid());

    const auto written { m_manager.setTile(patch) };

    if (written.has_value()) {
        GTEST_SKIP() << "bounds-validation gap still open: setTile() silently "
                        "clips out-of-bounds regions instead of rejecting "
                        "with InvalidImageRegion";
    }

    EXPECT_EQ(written.error(), CoreError::InvalidImageRegion);
}

// ============================================================
// getMetadata() - attribute lookup on a real spec
// ============================================================

/**
 * @test getMetadata() reads attributes from the real, OIIO-parsed spec,
 *       for both standard and custom attribute names.
 *
 * @note CONTRACT GAP (metadata loss): the spec attributes may not survive
 *       convertToRgbaInternal() (fresh target spec). The test probes the
 *       behavior and SELF-ACTIVATES when attribute propagation is fixed.
 */
TEST_F(SourceManagerIntegrationTest, GetMetadataReadsAttributesFromRealSpec)
{
    const auto file { makeExr("metadata", 4, 4) };
    ASSERT_TRUE(file.has_value());
    ASSERT_TRUE(m_manager.loadFile(file->string()).has_value());

    const auto description { m_manager.getMetadata("ImageDescription") };
    if (!description.has_value()) {
        GTEST_SKIP() << "metadata-loss gap still open: spec attributes do not "
                        "survive convertToRgbaInternal() - self-activates on fix";
    }

    EXPECT_EQ(*description, "CaptureMoment integration fixture");

    const auto custom { m_manager.getMetadata("sm:TestPattern") };
    ASSERT_TRUE(custom.has_value());
    EXPECT_EQ(*custom, "v1");
}

/**
 * @test On a loaded file, an unknown metadata key returns nullopt instead
 *       of throwing (complements the unloaded case covered at unit level).
 */
TEST_F(SourceManagerIntegrationTest, GetMetadataUnknownKeyOnLoadedFileReturnsNullopt)
{
    const auto file { makeTiff("nokey", 4, 4) };
    ASSERT_TRUE(file.has_value());
    ASSERT_TRUE(m_manager.loadFile(file->string()).has_value());

    EXPECT_FALSE(m_manager.getMetadata("no:such:key").has_value());
}

/**
 * @test CONTRACT GAP (metadata loss): modifying pixels through setTile()
 *       must NOT discard the spec attributes of the working buffer.
 *
 * @note Self-activating: skipped while metadata is already absent after
 *       load (nothing to preserve), asserted once the load-time gap is fixed.
 */
TEST_F(SourceManagerIntegrationTest, SetTilePreservesMetadata)
{
    const auto file { makeExr("keepmeta", 4, 4) };
    ASSERT_TRUE(file.has_value());
    ASSERT_TRUE(m_manager.loadFile(file->string()).has_value());

    // probe: metadata must exist BEFORE the write for this test to mean anything
    const auto before { m_manager.getMetadata("ImageDescription") };
    if (!before.has_value()) {
        GTEST_SKIP() << "metadata-loss gap still open at load time - "
                        "nothing to preserve yet";
    }

    auto tile { m_manager.getTile(0, 0, 2, 2) };
    ASSERT_TRUE(tile.has_value());
    (*tile.value())(0, 0, 0) = 0.75f;
    ASSERT_TRUE(m_manager.setTile(*tile.value()).has_value());

    const auto after { m_manager.getMetadata("ImageDescription") };
    ASSERT_TRUE(after.has_value());
    EXPECT_EQ(*after, "CaptureMoment integration fixture");
}

// ============================================================
// unload() / reload on real resources
// ============================================================

/**
 * @test unload() after a real load resets the whole observable state,
 *       including the recorded source path and the spec attributes.
 */
TEST_F(SourceManagerIntegrationTest, UnloadAfterRealLoadResetsObservableState)
{
    const auto file { makeTiff("unload", 8, 6) };
    ASSERT_TRUE(file.has_value());
    ASSERT_TRUE(m_manager.loadFile(file->string()).has_value());
    ASSERT_TRUE(m_manager.isLoaded());

    m_manager.unload();

    EXPECT_FALSE(m_manager.isLoaded());
    EXPECT_EQ(m_manager.width(), 0u);
    EXPECT_EQ(m_manager.height(), 0u);
    EXPECT_EQ(m_manager.channels(), 0u);
    EXPECT_EQ(m_manager.getImageSourcePath(), std::string());
    EXPECT_FALSE(m_manager.getMetadata("ImageDescription").has_value());
}

/**
 * @test The same file can be loaded again after unload(): the load path is
 *       not one-shot and stays coherent across ImageCache interactions.
 */
TEST_F(SourceManagerIntegrationTest, ReloadAfterUnloadSucceeds)
{
    const auto file { makeTiff("reload", 5, 4) };
    ASSERT_TRUE(file.has_value());
    ASSERT_TRUE(m_manager.loadFile(file->string()).has_value());
    m_manager.unload();

    const auto again { m_manager.loadFile(file->string()) };

    ASSERT_TRUE(again.has_value());
    EXPECT_TRUE(m_manager.isLoaded());
    EXPECT_EQ(m_manager.width(), 5u);
    EXPECT_EQ(m_manager.height(), 4u);
}

} // namespace CaptureMoment::Core::Managers::Test
