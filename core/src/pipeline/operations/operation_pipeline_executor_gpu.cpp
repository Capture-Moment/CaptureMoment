/**
 * @file operation_pipeline_executor_gpu.cpp
 * @brief Implementation of OperationPipelineExecutorGPU.
 * @author CaptureMoment Team
 * @date 2026
 */

#include "pipeline/operations/operation_pipeline_executor_gpu.h"
#include "image_processing/gpu/working_image_gpu_halide.h"
#include "pipeline/helpers/halide_color_space.h"

#include <spdlog/spdlog.h>

namespace CaptureMoment::Core::Pipeline {

void OperationPipelineExecutorGPU::buildOperationChain(const Common::ImageDim width, const Common::ImageDim height)
{
  /*  Halide::Var x("x"), y("y"), c("c");

    // 1. Chunky input from Halide::ImageParam (x,y,c) -> (x,y,c)
    Halide::Func rgb_input("rgb_input");
    rgb_input(x, y, c) = m_input(x, y, c);

    // ====================================================================
    // OKLAB DIRECT (No Transpose) + OPERATIONS
    // ====================================================================
    Halide::Func oklab_image { ColorSpace::rgbToOklab(rgb_input, x, y, c) };
    Halide::Func ops_result { applyOperations(oklab_image, x, y, c) };
    Halide::Func final_rgb_output { ColorSpace::oklabToRgb(ops_result, x, y, c) };

    // ====================================================================
    // SCHEDULING & COMPILATION GPU
    // ====================================================================
    final_rgb_output.output_buffer().dim(0).set_stride(4);
    final_rgb_output.output_buffer().dim(2).set_stride(1);

    Halide::Target target { Config::AppConfig::getHalideTarget() };

    try {
        final_rgb_output.compile_jit(target);
        m_pipeline = Halide::Pipeline(final_rgb_output);
        m_chain_built = true;
        spdlog::info("[OperationPipelineExecutorGPU::buildOperationChain]: Pipeline compiled (Direct Chunky Oklab).");
    } catch (const Halide::CompileError& e) {
        spdlog::critical("[OperationPipelineExecutorGPU::buildOperationChain]: {}", e.what());
        m_chain_built = false;
    }*/
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

bool OperationPipelineExecutorGPU::executeOnHalideBuffer(Halide::Buffer<float>& input_buffer, Halide::Buffer<float>& output_buffer)
{
    if (!m_chain_built || !m_pipeline.defined()) {
        return true;
    }

    try {
        // 1. Bind the actual C++ buffer memory to the Halide ImageParam
        // This is extremely fast (pointer copy), no data duplication.
        m_input.set(input_buffer);
        // 2. Get the target for execution
        // CRITICAL: For GPU execution, realize() MUST receive the target parameter
        Halide::Target target { Config::AppConfig::getHalideTarget() };

        spdlog::debug("[OperationPipelineExecutorGPU::executeOnHalideBuffer]: Halide Target Architecture: {}",
                     target.to_string());

        // 3. Execute the pipeline on the correct device (CPU or GPU)
        m_pipeline.realize(output_buffer);
        return true;
    }
    catch (const Halide::RuntimeError& e) {
        spdlog::critical("[OperationPipelineExecutorGPU::executeOnHalideBuffer]: Halide Runtime Error: {}", e.what());
        return false;
    }
    catch (const std::exception& e) {
        spdlog::critical("[OperationPipelineExecutorGPU::executeOnHalideBuffer]: Unexpected exception: {}", e.what());
        return false;
    }
}
} // namespace CaptureMoment::Core::Pipeline
