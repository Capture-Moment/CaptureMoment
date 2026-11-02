/**
 * @file pixel_format.h
 * @brief Defines the pixel storage formats used throughout the CaptureMoment Core library.
 *
 * This header provides the @ref CaptureMoment::Core::Common::PixelFormat enum,
 * which specifies how pixel data is stored in memory, including the number of channels
 * and the data type per channel. It's primarily used within the @ref ImageRegion
 * structure to define the format of its pixel data buffer.
 *
 * @author CaptureMoment Team
 * @date 2025
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <utility>

namespace CaptureMoment::Core {

namespace Common {

/**
 * @enum PixelFormat
 * @brief Enumerates the supported pixel storage formats in memory.
 *
 * This enum defines the layout and data type for individual pixels.
 * It specifies both the number of channels (e.g., RGB vs. RGBA) and
 * the data type used for each channel's value (e.g., float32 vs. uint8).
 *
 * @note The @ref RGBA_F32 format is the recommended default for internal
 *       processing pipelines as it supports High Dynamic Range (HDR) data
 *       and preserves maximum precision during calculations.
 *
 * @see Common::ImageRegion::m_format
 */
enum class PixelFormat : std::uint8_t {
    /**
     * @brief 4-channel format: Red, Green, Blue, Alpha in 32-bit float.
     *
     * Each pixel is stored as 4 consecutive `float` values.
     * - **Size per pixel:** 16 bytes (4 channels * 4 bytes per float)
     * - **Value range:** Theoretically `[-inf, +inf]`, but typically `[0.0f, 1.0f]`
     *                    for normalized data. HDR values can exceed `[0.0f, 1.0f]`.
     * - **Usage:** Standard format for internal image processing pipelines
     *             where HDR support and high precision are required.
     *
     * @code
     * ImageRegion region;
     * region.format = PixelFormat::RGBA_F32;
     * region.channels = 4;
     * @endcode
     */
    RGBA_F32,

    /**
     * @brief 3-channel format: Red, Green, Blue in 32-bit float.
     *
     * Each pixel is stored as 3 consecutive `float` values.
     * - **Size per pixel:** 12 bytes (3 channels * 4 bytes per float)
     * - **Value range:** Typically `[0.0f, 1.0f]`.
     * - **Usage:** Common for image files that do not contain an alpha channel.
     */
    RGB_F32,
};

/**
 * @brief Returns the number of channels for a given PixelFormat.
 * @param pf The pixel format.
 * @return The number of channels (e.g., 3 for RGB, 4 for RGBA).
 *         Returns 0 if the format is invalid.
 */
[[nodiscard]] constexpr std::uint8_t getChannelCount(PixelFormat pf) noexcept
{
    switch(pf) {
    case PixelFormat::RGBA_F32:
        return 4;
    case PixelFormat::RGB_F32:
        return 3;
    default:
        std::unreachable();
    }
}

/**
 * @brief Returns the size in bytes of a single pixel for a given PixelFormat.
 * @param fmt The pixel format.
 * @return The size in bytes (e.g., 12 for RGB_F32, 16 for RGBA_F32).
 *         Returns 0 if the format is invalid.
 */
[[nodiscard]] constexpr std::size_t getPixelSizeInBytes(PixelFormat fmt) noexcept
{
    return static_cast<std::size_t>(getChannelCount(fmt)) * sizeof(float);
}

} // namespace Common

} // namespace CaptureMoment::Core
