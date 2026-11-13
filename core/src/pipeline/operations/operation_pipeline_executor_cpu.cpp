/**
 * @file operation_pipeline_executor_cpu.cpp
 * @brief Implementation of OperationPipelineExecutorCPU.
 * @author CaptureMoment Team
 * @date 2026
 */

#include "pipeline/operations/operation_pipeline_executor_cpu.h"
#include "image_processing/cpu/working_image_cpu_halide.h"

#include <spdlog/spdlog.h>
#include <algorithm>
#include <thread>
#include "config/app_config.h"


namespace CaptureMoment::Core::Pipeline {

AutoschedulerBackendConfig OperationPipelineExecutorCPU::backendConfig() const
{
    // hardware_concurrency() may legally return 0 -> clamp to at least 1.
    const auto cores { std::thread::hardware_concurrency() };

    AutoschedulerBackendConfig backend;
    backend.scheduler_name = "Adams2019";
    backend.plugin_name    = "autoschedule_adams2019";
    backend.log_prefix     = "[OperationPipelineExecutorCPU]";
    backend.target         =  Config::AppConfig::getHalideTarget();
    backend.extra["parallelism"] = std::to_string(std::max(1U, cores));
    backend.extra["beam_size"]   = "8";
    // Guard rail: caps the memory the schedule may materialize (interactive app).
    backend.extra["memory_limit"] = std::to_string(static_cast<long long>(1) << 30); // 1 GiB
    return backend;
}

bool OperationPipelineExecutorCPU::execute(ImageProcessing::IWorkingImageHardware& working_image)
{
    auto* cpu_impl{dynamic_cast<ImageProcessing::WorkingImageCPU_Halide*>(&working_image)};
    if (!cpu_impl || !cpu_impl->isHalideBufferValid() || !cpu_impl->isOriginalHalideBufferValid()) {
        spdlog::error("[OperationPipelineExecutorCPU::execute]: Invalid Halide buffer or CPU implementation.");
        return false;
    }
    return executeOnHalideBuffer(cpu_impl->getOriginalHalideBuffer(), cpu_impl->getHalideBuffer());
}

} // namespace CaptureMoment::Core::Pipeline
