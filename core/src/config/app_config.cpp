/**
 * @file app_config.cpp
 * @brief Implementation of AppConfig.
 * @author CaptureMoment Team
 * @date 2026
 */

#include "config/app_config.h"
#include <spdlog/spdlog.h>

namespace CaptureMoment::Core::Config {

// ============================================================
// Static Member Initialization
// ============================================================


Halide::Target AppConfig::s_halide_target = Halide::get_host_target();

// ============================================================
// Singleton Instance
// ============================================================

AppConfig& AppConfig::instance()
{
    // C++11 Magic Static: Thread-safe initialization.
    static AppConfig s_instance;
    return s_instance;
}

// ============================================================
// Backend Configuration
// ============================================================

void AppConfig::setProcessingBackend(Common::MemoryType backend)
{
    if (m_processing_backend != backend)
    {
        m_processing_backend = backend;

        const auto backend_str { (backend == Common::MemoryType::CPU_RAM)
                                     ? "CPU_RAM"
                                     : "GPU_MEMORY"
        };

        spdlog::info("[AppConfig::setProcessingBackend]: Processing backend changed to: {}", backend_str);
    }
}

Common::MemoryType AppConfig::getProcessingBackend() const noexcept
{
    return m_processing_backend;
}

// ============================================================
// Halide Target Configuration
// ============================================================

void AppConfig::setHalideTarget(const Halide::Target& target)
{
    s_halide_target = target;
    spdlog::info("[AppConfig::setHalideTarget]: Halide Target updated. Architecture: {}",
                 target.to_string());
}

const Halide::Target& AppConfig::getHalideTarget()
{
    return s_halide_target;
}

// ============================================================
// Testing Utilities
// ============================================================

#ifdef ENABLE_TESTS
void AppConfig::reset()
{
    spdlog::debug("[AppConfig] Resetting configuration to defaults.");

    // Reset backend to default
    m_processing_backend = Common::MemoryType::CPU_RAM;

    // Reset target to default host (removes GPU features)
    s_halide_target = Halide::get_host_target();
}
#endif

} // namespace CaptureMoment::Core::Config
