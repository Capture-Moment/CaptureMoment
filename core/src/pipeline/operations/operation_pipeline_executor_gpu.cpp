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


/**
 * Loads the Anderson2021 autoscheduler plugin exactly once per process.
 * Non-fatal on failure: if the autoscheduler is statically linked, no DLL
 * exists to dlopen and the registration is already active. If it is really
 * missing, apply_autoscheduler() fails later with a clear error.
 */
void ensureAndersonPluginLoaded()
{
    static std::once_flag flag;
    std::call_once(flag, [] {
        try {
            Halide::load_plugin("autoschedule_anderson2021");
            spdlog::info("[OperationPipelineExecutorGPU]: anderson2021 plugin loaded.");
        } catch (const std::exception& e) {
            spdlog::warn("[OperationPipelineExecutorGPU]: load_plugin(anderson2021) failed ({}). "
                         "Expected if anderson2021 is statically linked.", e.what());
        }
    });
}
void OperationPipelineExecutorGPU::buildOperationChain(const Common::ImageDim width, const Common::ImageDim height)
{
    ensureAndersonPluginLoaded();

    Halide::Var x("x"), y("y"), c("c");

    // Linear graph, zero manual scheduling directive anywhere:
    // RGB(A) -> Oklab -> fused adjustments -> RGB(A)
    Halide::Func oklab_image  { ColorSpace::proPhotoLinearToOklab(m_input, x, y, c) };
    Halide::Func ops_result   { applyOperations(oklab_image, x, y, c) };
    Halide::Func final_output { ColorSpace::oklabToProPhotoLinear(ops_result, x, y, c) };
    // Output layout: interleaved RGBA (chunky). Stride constraints are KEPT:
    // they describe the memory layout and guide vectorization decisions.
    final_output.output_buffer().dim(0).set_stride(4);
    final_output.output_buffer().dim(2).set_stride(1);

    // currently loaded image: exact by construction.
    const int w { static_cast<int>(width) };
    const int h { static_cast<int>(height) };
    m_input.set_estimates({{0, w}, {0, h}, {0, 4}});
    final_output.output_buffer().set_estimates({{0, w}, {0, h}, {0, 4}});

    const Halide::Target target { Config::AppConfig::getHalideTarget() };

    Halide::AutoschedulerParams params;
    params.name = "Anderson2021";
    try {
        m_pipeline = Halide::Pipeline(final_output);
        m_pipeline.apply_autoscheduler(target, params);
        m_pipeline.compile_jit(target);

        m_chain_built = true;
        spdlog::info("[OperationPipelineExecutorGPU::buildOperationChain]: Anderson2021 auto-scheduled and compiled "
                     "for {}x{}, (target={}).", w, h, target.to_string());
    }
    catch (const Halide::CompileError& e) {
        spdlog::critical("[OperationPipelineExecutorGPU::buildOperationChain]: CompileError: {}", e.what());
        m_chain_built = false;
    }
    catch (const Halide::RuntimeError& e) {
        spdlog::critical("[OperationPipelineExecutorGPU::buildOperationChain]: RuntimeError: {}", e.what());
        m_chain_built = false;
    }
    catch (const std::exception& e) {
        spdlog::critical("[OperationPipelineExecutorGPU::buildOperationChain]: Unexpected: {}", e.what());
        m_chain_built = false;
    }
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

        // 2. Execute the pipeline on the correct device
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
