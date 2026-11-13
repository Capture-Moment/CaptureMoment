/**
 * @file operation_pipeline_executor_gpu.cpp
 * @brief Implementation of OperationPipelineExecutorGPU.
 * @author CaptureMoment Team
 * @date 2026
 */

#include "pipeline/operations/operation_pipeline_executor_gpu.h"
#include "image_processing/gpu/working_image_gpu_halide.h"
#include "config/app_config.h"

#include <spdlog/spdlog.h>

namespace CaptureMoment::Core::Pipeline {

void OperationPipelineExecutorGPU::buildOperationChain(const Common::ImageDim width, const Common::ImageDim height)
{
    AutoschedulerBackendConfig backend;
    backend.scheduler_name = "Anderson2021";
    backend.plugin_name    = "autoschedule_anderson2021";
    backend.log_prefix     = "[OperationPipelineExecutorGPU]";
    backend.target         = Config::AppConfig::getHalideTarget();
    // No extra parameters for now: Anderson2021 defaults are appropriate
    // for the GPU target until GPU-specific tuning is needed.

    buildChain(width, height, backend);
}

bool OperationPipelineExecutorGPU::execute(ImageProcessing::IWorkingImageHardware& working_image)
{
    auto* gpu_impl{dynamic_cast<ImageProcessing::WorkingImageGPU_Halide*>(&working_image)};
    if (!gpu_impl || !gpu_impl->isHalideBufferValid() || !gpu_impl->isOriginalHalideBufferValid()) {
        spdlog::error("[OperationPipelineExecutorGPU::execute]: Invalid Halide buffer or GPU implementation.");
        return false;
    }
    return executeOnHalideBuffer(gpu_impl->getOriginalHalideBuffer(), gpu_impl->getHalideBuffer());
}

} // namespace CaptureMoment::Core::Pipeline
