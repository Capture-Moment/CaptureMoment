/**
 * @file core_initialization.cpp
 * @brief Implementation of Core library initialization.
 * @details Orchestrates backend registration and benchmarking.
 * @author CaptureMoment Team
 * @date 2026
 */

#include "core_initialization.h"
#include "image_processing/registration/working_image_registration.h"
#include "image_processing/deciders/benchmarking_backend_decider.h"
#include "config/app_config.h"
#include <spdlog/spdlog.h>
#include <spdlog/cfg/env.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <mutex>

namespace CaptureMoment::Core {

namespace {
// Global flag used by std::call_once to ensure initialization runs only once.
inline std::once_flag s_init_flag;


// ============================================================
// Helper: Logging Initialization
// ============================================================
void init_logging()
{
    try {
        auto console_logger { spdlog::get("CaptureMoment") };

        if (!console_logger) {
            console_logger = spdlog::stdout_color_mt("CaptureMoment");
            if (!console_logger) {
                std::cerr << "Failed to create spdlog logger." << std::endl;
                return;
            }
            spdlog::info("CaptureMoment Logger created and registered.");
        } else {
            spdlog::info("CaptureMoment Logger already exists, retrieving it.");
        }


        // 2. Set Default Level (Can be overridden by env var or programmatic call)
        console_logger->set_level(spdlog::level::trace);

        // 3. Optional: Load log levels from environment variable (Uses SPDLOG_LEVEL by default)
        // This will override the level set above if the env var is present.
        spdlog::cfg::load_env_levels(); // This checks the SPDLOG_LEVEL environment variable

        // 4. Register the logger and set it as the default
        spdlog::set_default_logger(console_logger);

        // 5. Confirmation log (now using the configured logger)
        spdlog::info("CaptureMoment Logger initialized. Level: {}", spdlog::level::to_string_view(spdlog::default_logger()->level()));

    } catch (const spdlog::spdlog_ex& ex) {
        std::cerr << "Failed to initialize spdlog: " << ex.what() << std::endl;
    }
}

// ============================================================
// Helper: Programmatically Set Log Level
// ============================================================
void set_log_level(const char* level_str)
{
    if (!level_str) return;

    // Convert input string to lowercase for case-insensitive matching
    std::string level_str_lower = level_str;
    std::transform(level_str_lower.begin(), level_str_lower.end(), level_str_lower.begin(), ::tolower);

    // Get the current default logger
    auto logger = spdlog::default_logger();
    if (!logger) return;

    // Map string to spdlog level enum
    if (level_str_lower == "trace") {
        logger->set_level(spdlog::level::trace);
    }
    else if (level_str_lower == "debug") {
        logger->set_level(spdlog::level::debug);
    }
    else if (level_str_lower == "info") {
        logger->set_level(spdlog::level::info);
    }
    else if (level_str_lower == "warn") {
        logger->set_level(spdlog::level::warn);
    }
    else if (level_str_lower == "error") {
        logger->set_level(spdlog::level::err);
    }
    else if (level_str_lower == "off") {
        logger->set_level(spdlog::level::off);
    }
    else {
        // If unknown string, fallback to info
        logger->set_level(spdlog::level::info);
    }
}

// ============================================================
// Helper: Backends Registration
// ============================================================
void init_backends()
{
    // Call the registration function located in ImageProcessing namespace
    // This function registers CPU_Halide and GPU_Halide factories.
    CaptureMoment::Core::ImageProcessing::registerDefaultBackends();
}

// ============================================================
// Helper: Backend Decider & Configuration
// ============================================================
void init_backend_decider()
{
    spdlog::info("[CoreInitialization] Starting backend selection benchmark...");

    // 1. Create the decider
    CaptureMoment::Core::ImageProcessing::BenchmarkingBackendDecider benchmark_decider;

    // 2. Run benchmark (CPU vs GPU comparison)
    auto backend { benchmark_decider.decide() };

    // 3. Store the selected MemoryType in global Config
    CaptureMoment::Core::Config::AppConfig::instance().setProcessingBackend(backend);

    // 4. Store the specific Halide Target (Host+CUDA, Host+Vulkan, etc.) in Config
    // This allows WorkingImageGPU_Halide to retrieve the correct target.
    CaptureMoment::Core::Config::AppConfig::setHalideTarget(benchmark_decider.getWinningTarget());

    spdlog::info("[CoreInitialization] Backend configuration complete.");
}

void debug_mode()
{
    // Halide Debug
    #if defined(_WIN32)
    _putenv("HL_DEBUG_AUTOSCHEDULE=2");
    #else
    setenv("HL_DEBUG_AUTOSCHEDULE", "2", 1);
    #endif
}
// ============================================================
// Main Initialization Logic
// ============================================================
void perform_initialization()
{
    // Step 1: Initialize Logging
    init_logging();

    // Step 2: Register Factories
    init_backends();

    // Step 3: Run Benchmark Decider & Setup Config
    init_backend_decider();

    // Step 4: Optional Debug Mode
    debug_mode();
}

} 

void initialize()
{
    // std::call_once ensures that 'perform_initialization' is executed exactly once.
    // It is thread-safe: if multiple threads call initialize(), they will synchronize,
    // and only one will execute the registration and benchmarking.
    std::call_once(s_init_flag, perform_initialization);
}

} // namespace CaptureMoment::Core
