/**
 * @file source_manager_test.cpp
 * @brief PURE UNIT tests for the SourceManager state machine (no I/O).
 *
 * @details
 * Unit tests only: no files, no TIFF fixtures, no decoding, no pixel
 * conversion, no integration with the real OIIO pipeline. The suite
 * exercises ONLY the manager's own logic on a never-loaded manager:
 *   - initial state after construction
 *   - guard order of every public method when no image is loaded
 *   - empty-path guard of loadFile()
 *   - unload() idempotence and state cleanliness
 *
 * Honest scope note: SourceManager cannot be instantiated without OIIO
 * (its constructor fetches the global ImageCache), so the library stays
 * linked and initialized - but no test performs any real I/O or image
 * processing.
 *
 * Deliberately NOT covered here (future INTEGRATION suite, to be added
 * step by step when wanted):
 *   - real file loading and RGBA_F32 standardization
 *   - getTile()/setTile() pixel content and round trips
 *   - getMetadata() attribute lookup on a real spec
 *   - the metadata-loss and bounds-validation contract gaps
 *
 * @author CaptureMoment Team
 * @date 2026
 */

#include "managers/source_manager.h"

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace CaptureMoment::Core::Managers::Test {

using CaptureMoment::Core::Common::ImageRegion;
using CaptureMoment::Core::ErrorHandling::CoreError;
using CaptureMoment::Core::Managers::SourceManager;

// ============================================================
// Fixture: one fresh (never-loaded) SourceManager per test
// ============================================================

/**
 * @brief Fixture for the unit-level contract: a freshly constructed
 *        manager, nothing loaded, no filesystem activity.
 */

class SourceManagerUnitTest: public ::testing::Test
{
    protected:
        SourceManager m_manager; // one fresh manager per test
};
// ============================================================
// Initial state - freshly constructed manager
// ============================================================

/**
 * @test A freshly constructed manager reports no loaded image.
 */
TEST_F(SourceManagerUnitTest, FreshManagerIsNotLoaded)
{
    EXPECT_FALSE(m_manager.isLoaded());
}

/**
 * @test Without a loaded image, the dimension accessors return 0
 *       (documented contract of ISourceManager).
 */
TEST_F(SourceManagerUnitTest, FreshManagerReportsZeroDimensions)
{
    EXPECT_EQ(m_manager.width(), 0u);
    EXPECT_EQ(m_manager.height(), 0u);
    EXPECT_EQ(m_manager.channels(), 0u);
}

/**
 * @test The source path is empty until a load succeeds.
 */
TEST_F(SourceManagerUnitTest, FreshManagerHasEmptySourcePath)
{
    EXPECT_EQ(m_manager.getImageSourcePath(), std::string());
}

// ============================================================
// loadFile() - Empty-path guard (pure logic, no OIIO involved)
// ============================================================

/**
 * @test An empty path is rejected with FileNotFound before any state is
 *       touched: the manager stays unloaded.
 */
TEST_F(SourceManagerUnitTest, LoadFileWithEmptyPathReturnsFileNotFound)
{
    const auto result { m_manager.loadFile("") };

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), CoreError::FileNotFound);
    EXPECT_FALSE(m_manager.isLoaded());
}

/**
 * @test A rejected loadFile("") leaves the whole observable state
 *       untouched (no path, no dimensions).
 */
TEST_F(SourceManagerUnitTest, LoadFileWithEmptyPathDoesNotChangeState)
{
    const auto result { m_manager.loadFile("") };

    ASSERT_FALSE(result.has_value());
    EXPECT_FALSE(m_manager.isLoaded());
    EXPECT_EQ(m_manager.width(), 0u);
    EXPECT_EQ(m_manager.height(), 0u);
    EXPECT_EQ(m_manager.channels(), 0u);
    EXPECT_EQ(m_manager.getImageSourcePath(), std::string());
}

// ============================================================
// unload() - Idempotence and state cleanliness
// ============================================================

/**
 * @test unload() on a manager that never loaded anything is a safe no-op.
 */
TEST_F(SourceManagerUnitTest, UnloadWithoutImageIsSafeNoOp)
{
    m_manager.unload();

    EXPECT_FALSE(m_manager.isLoaded());
    EXPECT_EQ(m_manager.getImageSourcePath(), std::string());
}

/**
 * @test unload() is idempotent: calling it twice keeps the state clean.
 */
TEST_F(SourceManagerUnitTest, UnloadIsIdempotent)
{
    m_manager.unload();
    m_manager.unload();

    EXPECT_FALSE(m_manager.isLoaded());
    EXPECT_EQ(m_manager.width(), 0u);
    EXPECT_EQ(m_manager.height(), 0u);
    EXPECT_EQ(m_manager.channels(), 0u);
}

// ============================================================
// Guard ORDER - "not loaded" takes precedence over other errors
// ============================================================

/**
 * @test getTile() without a loaded image fails with SourceNotLoaded.
 */
TEST_F(SourceManagerUnitTest, GetTileWithoutLoadedImageReturnsSourceNotLoaded)
{
    const auto result { m_manager.getTile(0, 0, 16, 16) };

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), CoreError::SourceNotLoaded);
}

/**
 * @test Guard order: an INVALID tile request (zero size) on an unloaded
 *       manager still reports SourceNotLoaded - the loaded-check runs
 *       BEFORE the size-check in getTile().
 */
TEST_F(SourceManagerUnitTest, GetTileZeroSizeWithoutLoadedImageStillReturnsSourceNotLoaded)
{
    const auto result { m_manager.getTile(0, 0, 0, 0) };

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), CoreError::SourceNotLoaded);
}

/**
 * @test setTile() without a loaded image fails with SourceNotLoaded.
 */
TEST_F(SourceManagerUnitTest, SetTileWithoutLoadedImageReturnsSourceNotLoaded)
{
    std::vector<float> data(8 * 8 * 4, 0.5f);
    const ImageRegion tile(std::move(data), 8, 8, 4);
    ASSERT_TRUE(tile.isValid()); // the tile itself is fine - only the manager is empty

    const auto result { m_manager.setTile(tile) };

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), CoreError::SourceNotLoaded);
}

/**
 * @test Guard order: an INVALID region on an unloaded manager still
 *       reports SourceNotLoaded - the loaded-check runs BEFORE the
 *       isValid() check in setTile().
 */
TEST_F(SourceManagerUnitTest, SetTileInvalidRegionWithoutLoadedImageStillReturnsSourceNotLoaded)
{
    // 2 values for an 8x8x4 region: isValid() == false
    std::vector<float> data{1.0f, 2.0f};
    const ImageRegion invalid(std::move(data), 8, 8, 4);
    ASSERT_FALSE(invalid.isValid());

    const auto result { m_manager.setTile(invalid) };

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), CoreError::SourceNotLoaded);
}

/**
 * @test getMetadata() without a loaded image returns nullopt for any key
 *       (no spec to query, no throw).
 */
TEST_F(SourceManagerUnitTest, GetMetadataWithoutLoadedImageReturnsNullopt)
{
    EXPECT_FALSE(m_manager.getMetadata("anything").has_value());
}

} // namespace CaptureMoment::Core::Managers::Test
