/**
 * @file image_region_test.cpp
 * @brief Unit tests for CaptureMoment::Core::Common::ImageRegion.
 *
 * @details
 * Pure unit tests: no I/O, no OIIO, no mock required. They validate the
 * value-type contract documented in CORE_DESIGN.md:
 *   - constructors (allocation, move, span-copy, default)
 *   - isValid() integrity checks (zero dims, channel guard > 8,
 *     overflow-safe size computation, buffer consistency)
 *   - getBuffer() zero-copy span access
 *   - getMdSpan() 3D access
 *   - operator()(y, x, c) pixel access following the Chunky layout
 *   - public coordinate members (m_x / m_y) assignment
 *
 * @note ImageRegion deliberately has NO raw-pointer constructor: pixel
 *       buffers enter either by std::vector (ownership move) or by
 *       std::span (copy). Tests must use these two entry points - the
 *       absence of raw pointers IS part of the memory-safety contract.
 *
 * @author CaptureMoment Team
 * @date 2026
 */

#include "common/image_region.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <numeric>
#include <span>
#include <vector>

namespace CaptureMoment::Core::Common::Test {

using CaptureMoment::Core::Common::ImageRegion;
using CaptureMoment::Core::Common::PixelFormat;

// ============================================================
// C++23 Concepts - Compile-time contract checks
// ============================================================

// ImageRegion must satisfy the generic-image concepts it documents
// (verified at compile time - no runtime cost)
static_assert(CaptureMoment::Core::Common::ImageLike<ImageRegion>,
              "ImageRegion must satisfy ImageLike");
static_assert(CaptureMoment::Core::Common::MutableImageLike<ImageRegion>,
              "ImageRegion must satisfy MutableImageLike");

// ============================================================
// Constructors - Allocation, defaults & coordinates
// ============================================================

/**
 * @test The allocation constructor (w, h, ch) reserves exactly
 *       width*height*channels floats and produces a valid region.
 */
TEST(ImageRegion, AllocationConstructorAllocatesExactSize)
{
    const ImageRegion region(8, 6, 4); // buffer intentionally uninitialized

    EXPECT_TRUE(region.isValid());
    EXPECT_EQ(region.width(), 8u);
    EXPECT_EQ(region.height(), 6u);
    EXPECT_EQ(region.getDataSize(), 8u * 6u * 4u);
    EXPECT_EQ(region.sizeInBytes(), 8u * 6u * 4u * sizeof(float));
}

/**
 * @test The default constructor creates an empty, invalid region
 *       (documented contract).
 */
TEST(ImageRegion, DefaultConstructorIsInvalid)
{
    const ImageRegion region; // 0x0x0, empty buffer
    EXPECT_FALSE(region.isValid());
}

/**
 * @test The move constructor accepts tile coordinates (x, y) and the
 *       format defaults to RGBA_F32.
 */
TEST(ImageRegion, ConstructorInitializesCoordinatesAndFormat)
{
    std::vector<float> data(4 * 4 * 4, 0.0f);
    const ImageRegion tile(std::move(data), 4, 4, 4, /*x=*/2, /*y=*/3);

    EXPECT_EQ(tile.x(), 2);
    EXPECT_EQ(tile.y(), 3);
    EXPECT_EQ(tile.format(), PixelFormat::RGBA_F32);
}

/**
 * @test The move constructor produces a valid, correctly sized region:
 *       ownership of the vector is transferred, no copy is made.
 */
TEST(ImageRegion, MoveConstructorProducesValidRegion)
{
    std::vector<float> data(64 * 64 * 4, 0.5f);
    const ImageRegion region(std::move(data), 64, 64, 4);

    EXPECT_TRUE(region.isValid());
    EXPECT_EQ(region.width(), 64u);
    EXPECT_EQ(region.height(), 64u);
    EXPECT_EQ(region.channels(), 4u);
    EXPECT_EQ(region.getDataSize(), 64u * 64u * 4u);
}

/**
 * @test Constructing from a non-owning span copies the data: later
 *       modifications of the source buffer must not leak into the region.
 */
TEST(ImageRegion, SpanConstructorCopiesSourceData)
{
    std::vector<float> source(2 * 2 * 4, 1.0f);

    ImageRegion region(std::span<const float>{source}, 2, 2, 4);
    ASSERT_TRUE(region.isValid());

    source[0] = 99.0f; // mutate the source after construction
    EXPECT_FLOAT_EQ(region(0, 0, 0), 1.0f);
}

/**
 * @test A region built from a correctly sized span is valid and exposes
 *       its geometry through the safe accessors.
 */
TEST(ImageRegion, SpanConstructorExposesGeometry)
{
    std::vector<float> data(64 * 64 * 4, 0.5f);
    const ImageRegion region(std::span<const float>{data}, 64, 64, 4);

    EXPECT_TRUE(region.isValid());
    EXPECT_EQ(region.width(), 64u);
    EXPECT_EQ(region.height(), 64u);
    EXPECT_EQ(region.channels(), 4u);
}

// ============================================================
// isValid() - Region integrity guards
// ============================================================

/**
 * @test isValid() rejects zero width (branch 1 of the integrity check).
 */
TEST(ImageRegion, ZeroWidthRegionIsInvalid)
{
    std::vector<float> data(10 * 4, 1.0f);
    const ImageRegion region(std::move(data), 0, 10, 4);
    EXPECT_FALSE(region.isValid());
}

/**
 * @test isValid() rejects zero height (other half of branch 1).
 */
TEST(ImageRegion, ZeroHeightRegionIsInvalid)
{
    std::vector<float> data(64 * 4, 1.0f);
    const ImageRegion region(std::move(data), 64, 0, 4);
    EXPECT_FALSE(region.isValid());
}

/**
 * @test isValid() rejects a zero channel count (branch 1 of the check).
 */
TEST(ImageRegion, ZeroChannelsRegionIsInvalid)
{
    // Move constructor: no allocation is driven by the dimensions,
    // so an inconsistent (w, h, ch) trio is safe to build here.
    std::vector<float> data(10 * 4, 1.0f);
    const ImageRegion region(std::move(data), 64, 64, 0);
    EXPECT_FALSE(region.isValid());
}

/**
 * @test isValid() rejects absurd channel counts (documented guard: > 8).
 */
TEST(ImageRegion, TooManyChannelsIsInvalid)
{
    std::vector<float> data(2 * 2 * 9, 1.0f);
    const ImageRegion region(std::move(data), 2, 2, 9);
    EXPECT_FALSE(region.isValid());
}

/**
 * @test ImageChan is uint8_t: a negative channel count silently wraps
 *       around (e.g. -1 becomes 255) and must fall into the > 8 guard.
 */
TEST(ImageRegion, ChannelCountWraparoundIsInvalid)
{
    std::vector<float> data(10 * 4, 1.0f);
    const ImageRegion region(std::move(data), 64, 64, static_cast<ImageChan>(-1));
    EXPECT_FALSE(region.isValid());
}

/**
 * @test ImageDim is size_t: a negative dimension silently wraps around
 *       to a huge value. The overflow-safe size computation must reject
 *       it (branch 2/3 of the integrity check) instead of allocating.
 */
TEST(ImageRegion, DimensionWraparoundIsInvalid)
{
    // Move constructor again: no allocation is performed from (w, h, ch).
    std::vector<float> data(10 * 4, 1.0f);
    const ImageRegion region(std::move(data), static_cast<ImageDim>(-64), 64, 4);
    EXPECT_FALSE(region.isValid());
}

/**
 * @test isValid() must reject a region whose buffer size does not match
 *       width * height * channels (corrupted / inconsistent data).
 */
TEST(ImageRegion, MismatchedDataSizeIsInvalid)
{
    // 2 values for a 4x4x4 region: expected 64 elements
    std::vector<float> data{1.0f, 2.0f};
    const ImageRegion region(std::move(data), 4, 4, 4);

    EXPECT_FALSE(region.isValid());
}

/**
 * @test An empty buffer with non-zero dimensions is invalid
 *       (remaps the "null data" case: with the vector/span API there is
 *       no null pointer, an absent buffer is an empty vector).
 */
TEST(ImageRegion, EmptyBufferWithNonZeroDimensionsIsInvalid)
{
    std::vector<float> data; // empty
    const ImageRegion region(std::move(data), 64, 64, 4);
    EXPECT_FALSE(region.isValid());
}

// ============================================================
// getter accessors - getBuffer() and getMdSpan()
// ============================================================

/**
 * @test getBuffer() returns a span of exactly width*height*channels
 *       elements, and writing through the span is visible through
 *       operator() (same underlying memory).
 */
TEST(ImageRegion, GetBufferExposesUnderlyingData)
{
    std::vector<float> data(2 * 2 * 4, 0.0f);
    ImageRegion region(std::move(data), 2, 2, 4);

    auto buffer { region.getBuffer() };
    ASSERT_EQ(buffer.size(), 16u);

    buffer[0] = 0.25f;
    EXPECT_FLOAT_EQ(region(0, 0, 0), 0.25f);
}

/**
 * @test getMdSpan() exposes the same memory as a 3D mdspan indexed as
 *       (y, x, c), consistent with the Chunky layout.
 */
TEST(ImageRegion, GetMdSpanProvidesThreeDimensionalAccess)
{
    std::vector<float> data(2 * 3 * 4);
    std::iota(data.begin(), data.end(), 0.0f);
    ImageRegion region(std::move(data), 2, 3, 4); // w=2, h=3, ch=4

    auto view { region.getMdSpan() }; // extents: (height, width, channels)
    EXPECT_FLOAT_EQ((view[0, 0, 0]), 0.0f);
    EXPECT_FLOAT_EQ((view[0, 1, 0]), 4.0f);
    EXPECT_FLOAT_EQ((view[1, 0, 0]), 8.0f);
    EXPECT_FLOAT_EQ((view[2, 1, 3]), 23.0f); // last element
}

// ============================================================
// operator()(y, x, c) - Chunky pixel layout
// ============================================================

/**
 * @test Pixel indexing follows the internal Chunky (interleaved) layout
 *       documented in IMAGE_PIPELINE.md:
 *           index(y, x, c) = (y * width + x) * channels + c
 *       [R0, G0, B0, A0, R1, G1, B1, A1, ...]
 */
TEST(ImageRegion, PixelAccessFollowsChunkyLayout)
{
    std::vector<float> data(3 * 2 * 4);
    std::iota(data.begin(), data.end(), 0.0f); // 0, 1, 2, 3, 4, ...

    const ImageRegion region(std::move(data), 2, 3, 4);

    EXPECT_FLOAT_EQ(region(0, 0, 0), 0.0f); // pixel (x=0, y=0) R
    EXPECT_FLOAT_EQ(region(0, 0, 3), 3.0f); // pixel (x=0, y=0) A
    EXPECT_FLOAT_EQ(region(0, 1, 0), 4.0f); // pixel (x=1, y=0) R
    EXPECT_FLOAT_EQ(region(1, 0, 0), 8.0f); // pixel (x=0, y=1) R
}

// ============================================================
// Coordinates - public members (m_x / m_y) assignment
// ============================================================

/**
 * @test Assigning to the public coordinate members updates the values,
 *       readable both directly and through the x() / y() accessors.
 *       This is exactly how SourceManager positions a tile
 *       (see source_manager.cpp: region->m_x = ...).
 */
TEST(ImageRegion, CoordinateMemberAssignment)
{
    std::vector<float> data(2 * 2 * 4, 0.0f);
    ImageRegion tile(std::move(data), 2, 2, 4);

    tile.m_x = 1;
    tile.m_y = 1;

    EXPECT_EQ(tile.m_x, 1);
    EXPECT_EQ(tile.m_y, 1);
    EXPECT_EQ(tile.x(), 1);
    EXPECT_EQ(tile.y(), 1);
}

} // namespace CaptureMoment::Core::Common::Test
