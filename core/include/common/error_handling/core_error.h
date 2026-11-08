/**
 * @file core_error.h
 * @brief Central error handling system for CaptureMoment Core library.
 * @details Defines error codes and categories using memory-efficient enums.
 *          This module provides the types used for `std::expected` return values
 *          throughout the library.
 */

#pragma once

#include <string_view>
#include <cstdint>
#include <format>

namespace CaptureMoment::Core {

/**
 * @namespace ErrorHandling
 * @brief Contains the error handling infrastructure for the Core library.
 *
 * This namespace defines the core error enumeration (`CoreError`) and its
 * categorization (`CoreErrorCategory`), along with utility functions
 * for string conversion and classification.
 */
namespace ErrorHandling {

/**
 * @brief Enumeration of all error codes for CaptureMoment Core.
 * @details Inherits from `uint8_t` to minimize memory footprint (1 byte instead of 4).
 *
 *          This enum is used as the error type in `std::expected<T, CoreError>`.
 *          Values represent specific failure scenarios across Image Processing,
 *          Source Management, I/O, and System operations.
 */
enum class CoreError : uint8_t {
    // ==========================================
    // Success State
    // ==========================================

    /**
     * @brief Operation completed successfully.
     */
    Success = 0,

    // ==========================================
    // Image Processing Errors
    // ==========================================

    /**
     * @brief The provided ImageRegion dimensions or data are invalid.
     * @details Typically raised when width <= 0, height <= 0, or bounds are out of range.
     */
    InvalidImageRegion = 1,

    /**
     * @brief Memory allocation failed.
     * @details Raised when a CPU (`std::bad_alloc`) or GPU allocation fails during buffer creation.
     */
    AllocationFailed = 2,

    /**
     * @brief Halide buffer is invalid or undefined.
     * @details The underlying buffer handle is null or improperly initialized.
     */
    InvalidHalideBuffer = 3,

    /**
     * @brief The working image state is invalid or corrupted.
     * @details Raised when the internal working image data cannot be processed or is missing.
     */
    InvalidWorkingImage = 4,

    // ==========================================
    // I/O Errors
    // ==========================================

    /**
     * @brief Generic File I/O error.
     * @details Covers file system level errors such as permission denied or general read/write failures.
     */
    IOError = 5,

    /**
     * @brief The specified file path does not exist.
     * @details Raised when attempting to load a file that is not present on the disk.
     */
    FileNotFound = 6,

    /**
     * @brief The file format is not supported by OIIO or the system.
     * @details Raised when the file extension or header signature is unrecognized.
     */
    UnsupportedFormat = 7,

    /**
     * @brief Failed to decode the image data.
     * @details The file exists but its content is corrupt or does not match the expected specification.
     */
    DecodingError = 8,

    /**
     * @brief Attempted to access image data without loading a file first.
     * @details Raised when calling `getTile` or `setTile` on a `SourceManager` with no active image.
     */
    SourceNotLoaded = 9,

    // ==========================================
    // System Errors
    // ==========================================

    /**
     * @brief An unexpected error occurred.
     * @details Catch-all for logic errors, undefined states, or unhandled exceptions.
     */
    Unexpected = 99
};

/**
 * @brief Enumeration of high-level error categories.
 * @details Used for filtering logs, metrics, or dispatching recovery strategies.
 *          Errors are grouped by the subsystem they affect most directly.
 */
enum class CoreErrorCategory : uint8_t {
    /**
     * @brief General system or unknown errors.
     */
    System = 0,

    /**
     * @brief Errors related to Image Processing, GPU, or Memory.
     */
    ImageProcessing = 1,

    /**
     * @brief Errors related to Serialization, File I/O, or Network.
     */
    Serialization = 2,

    /**
     * @brief General errors applicable to multiple subsystems.
     */
    Common = 3,

    /**
     * @brief Specific to SourceManager, file loading, and caching.
     */
    Source = 4
};


// ============================================================
// Helpers (Conversion functions)
// ============================================================

/**
 * @brief Converts a `CoreError` code to its high-level Category.
 *
 * @param code The error code to categorize.
 * @return CoreErrorCategory The corresponding category.
 */
[[nodiscard]] constexpr CoreErrorCategory get_error_category(CoreError code)
{
    switch (code)
    {
    case CoreError::InvalidImageRegion:
        return CoreErrorCategory::Common;

    case CoreError::AllocationFailed:
    case CoreError::InvalidHalideBuffer:
    case CoreError::InvalidWorkingImage:
        return CoreErrorCategory::ImageProcessing;

    case CoreError::IOError:
    case CoreError::FileNotFound:
    case CoreError::UnsupportedFormat:
    case CoreError::SourceNotLoaded:
        return CoreErrorCategory::Source;

    case CoreError::DecodingError:
        return CoreErrorCategory::Serialization;

    case CoreError::Success:
    case CoreError::Unexpected:
    default:
        return CoreErrorCategory::System;
    }
}

/**
 * @brief Converts a `CoreError` code to a human-readable string.
 * @note Marked `constexpr` for potential compile-time optimization, but callable at runtime.
 *
 * @param code The error code.
 * @return std::string_view String representation of the error.
 */
[[nodiscard]] constexpr std::string_view to_string(CoreError code)
{
    switch (code)
    {
    case CoreError::Success: return "Success";
    case CoreError::InvalidImageRegion: return "InvalidImageRegion";
    case CoreError::AllocationFailed: return "AllocationFailed";
    case CoreError::InvalidHalideBuffer: return "InvalidHalideBuffer";
    case CoreError::InvalidWorkingImage: return "InvalidWorkingImage";
    case CoreError::IOError: return "IOError";
    case CoreError::Unexpected: return "Unexpected";
    case CoreError::FileNotFound:  return "FileNotFound";
    case CoreError::UnsupportedFormat: return "UnsupportedFormat";
    case CoreError::SourceNotLoaded: return "SourceNotLoaded";
    case CoreError::DecodingError: return "DecodingError";
    default: return "Unknown";
    }
}

/**
 * @brief Converts a `CoreErrorCategory` to a human-readable string.
 *
 * @param category The error category.
 * @return std::string_view String representation of the category.
 */
[[nodiscard]] constexpr std::string_view to_string(CoreErrorCategory category)
{
    switch (category)
    {
    case CoreErrorCategory::System: return "System";
    case CoreErrorCategory::ImageProcessing: return "ImageProcessing";
    case CoreErrorCategory::Serialization: return "Serialization";
    case CoreErrorCategory::Common: return "Common";
    case CoreErrorCategory::Source: return "Source";

    default: return "Unknown";
    }
}

} // namespace ErrorHandling

} // namespace CaptureMoment::Core

/**
 * @brief std::formatter specialization for CoreError.
 * @details Enables direct formatting with spdlog/std::format (e.g., "{}", CoreError::FileNotFound).
 */
template <>
struct std::formatter<CaptureMoment::Core::ErrorHandling::CoreError>
    : std::formatter<std::string_view>
{
    auto format(CaptureMoment::Core::ErrorHandling::CoreError code, auto& ctx) const
    {
        return std::formatter<std::string_view>::format(
            CaptureMoment::Core::ErrorHandling::to_string(code), ctx);
    }
};
