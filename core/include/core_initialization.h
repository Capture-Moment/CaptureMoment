/**
 * @file core_initialization.h
 * @brief Core library initialization entry point.
 *
 * This header provides the single public function required to initialize the
 * CaptureMoment Core library (Backends Registration, Benchmarking).
 *
 * @author CaptureMoment Team
 * @date 2026
 */

#pragma once

namespace CaptureMoment::Core {

/**
 * @brief Initializes the CaptureMoment Core library.
 *
 * @details
 * This function handles the complete startup sequence for the image processing core.
 * It uses a thread-safe `std::call_once` mechanism to ensure that
 * the heavy initialization logic (registration + benchmarking) is executed
 * exactly once during the application's lifetime.
 *
 * **Initialization Sequence:**
 * 1. Registers CPU/GPU backends into `WorkingImageFactory` (via `ImageProcessing::registerDefaultBackends`).
 * 2. Runs `BenchmarkingBackendDecider` to select optimal hardware (CPU vs GPU).
 * 3. Stores the selected backend in `AppConfig`.
 *
 *
 * @note This function is thread-safe.
 */
void initialize();

/**
 * @brief Sets the log level for the Core library.
 *
 * @param level_str A string representing the desired log level (e.g., "trace", "debug", "info", "warn", "error", "critical", "off").
 *
 * @details
 * This function allows dynamic adjustment of the logging verbosity for the Core library.
 * It can be called at any time after initialization to change the log level on-the-fly.
 *
 * @note The log level string is case-insensitive. If an invalid string is provided, it defaults to "info".
 */
void set_log_level(const char* level_str);

} // namespace CaptureMoment::Core
