#include "pipeline/operations/operation_pipeline_executor_cpu.h"
#include "image_processing/cpu/working_image_cpu_halide.h"
#include "pipeline/helpers/halide_color_space.h"

#include <spdlog/spdlog.h>
#include <algorithm>
#include <cstdint>
#include <mutex>
#include <thread>

namespace CaptureMoment::Core::Pipeline {

namespace {

/**
 * Loads the Adams2019 autoscheduler plugin exactly once per process.
 * Non-fatal on failure: if the autoscheduler is statically linked, no DLL
 * exists to dlopen and the registration is already active. If it is really
 * missing, apply_autoscheduler() fails later with a clear error.
 */
void ensureAdamsPluginLoaded()
{
    static std::once_flag flag;
    std::call_once(flag, [] {
        try {
            Halide::load_plugin("autoschedule_adams2019");
            spdlog::info("[OperationPipelineExecutorCPU]: Adams2019 plugin loaded.");
        } catch (const std::exception& e) {
            spdlog::warn("[OperationPipelineExecutorCPU]: load_plugin(adams2019) failed ({}). "
                         "Expected if Adams2019 is statically linked.", e.what());
        }
    });
}

} // namespace

void OperationPipelineExecutorCPU::buildOperationChain()
{
    ensureAdamsPluginLoaded();

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

    // Adams2019 estimates (input and output). Using the dimensions of the
    // currently loaded image: exact by construction.
    const int w { m_image_width };
    const int h { m_image_height };
    m_input.set_estimates({{0, w}, {0, h}, {0, 4}});
    final_output.output_buffer().set_estimates({{0, w}, {0, h}, {0, 4}});

    const Halide::Target target { Halide::get_host_target() };

    Halide::AutoschedulerParams params;
    params.name = "Adams2019";

    // hardware_concurrency() may legally return 0 -> clamp to at least 1.
    const unsigned int cores { std::max(1u, std::thread::hardware_concurrency()) };
    params.extra["parallelism"] = std::to_string(cores);

    // Empty path = weights embedded in the autoscheduler build.
    // If your plugin build does NOT embed them, bundle baseline.weights as an
    // application resource and resolve its path at runtime (never a hardcoded
    // site-packages path).
   // params.extra["weights_path"] = "";

    // Guard rail: caps the memory the schedule may materialize (interactive app).
    // Bytes. Tune to the target machine class.
    params.extra["memory_limit"] = std::to_string(static_cast<long long>(1) << 30); // 1 GiB

    try {
        m_pipeline = Halide::Pipeline(final_output);
        m_pipeline.apply_autoscheduler(target, params);
        m_pipeline.compile_jit(target);

        m_chain_built = true;
        spdlog::info("[OperationPipelineExecutorCPU::buildOperationChain]: Adams2019 auto-scheduled and compiled "
                     "for {}x{} ({} cores, target={}).", w, h, cores, target.to_string());
    }
    catch (const Halide::CompileError& e) {
        spdlog::critical("[OperationPipelineExecutorCPU::buildOperationChain]: CompileError: {}", e.what());
        m_chain_built = false;
    }
    catch (const Halide::RuntimeError& e) {
        spdlog::critical("[OperationPipelineExecutorCPU::buildOperationChain]: RuntimeError: {}", e.what());
        m_chain_built = false;
    }
    catch (const std::exception& e) {
        spdlog::critical("[OperationPipelineExecutorCPU::buildOperationChain]: Unexpected: {}", e.what());
        m_chain_built = false;
    }
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

bool OperationPipelineExecutor::executeOnHalideBuffer(Halide::Buffer<float>& input_buffer, Halide::Buffer<float>& output_buffer)
{
    if (!m_chain_built || !m_pipeline.defined()) {
        return true;
    }

    try {
        // 1. Bind the actual C++ buffer memory to the Halide ImageParam
        // This is extremely fast (pointer copy), no data duplication.
        m_input.set(input_buffer);
        // 2. Get the target for execution
        Halide::Target target { Config::AppConfig::getHalideTarget() };

        spdlog::debug("[OperationPipelineExecutor::executeOnHalideBuffer]: Halide Target Architecture: {}",
                     target.to_string());

        // 3. Execute the pipeline on the correct device
        m_pipeline.realize(output_buffer);
        return true;
    }
    catch (const Halide::RuntimeError& e) {
        spdlog::critical("[OperationPipelineExecutor::executeOnHalideBuffer]: Halide Runtime Error: {}", e.what());
        return false;
    }
    catch (const std::exception& e) {
        spdlog::critical("[OperationPipelineExecutor::executeOnHalideBuffer]: Unexpected exception: {}", e.what());
        return false;
    }
}

} // namespace CaptureMoment::Core::Pipeline