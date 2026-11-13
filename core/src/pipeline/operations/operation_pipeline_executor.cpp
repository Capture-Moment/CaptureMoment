/**
 * @file operation_pipeline_executor.cpp
 * @brief Implementation of OperationPipelineExecutor.
 * @author CaptureMoment Team
 * @date 2026
 */

#include "pipeline/operations/operation_pipeline_executor.h"
#include "operations/interfaces/i_operation_fusion_logic.h"
#include "operations/operation_factory.h"
#include "config/app_config.h"

#include <spdlog/spdlog.h>
#include <mutex>

namespace CaptureMoment::Core::Pipeline {

OperationPipelineExecutor::OperationPipelineExecutor()

    : IHalidePipelineExecutor(),
      m_chain_built(false),
      m_factory(nullptr)
{
    m_input.dim(0).set_stride(4);
    m_input.dim(2).set_stride(1);

    spdlog::debug("[OperationPipelineExecutor::OperationPipelineExecutor]: Constructed. Input set to Float(32), 3 dimensions. Backend: {}",
                  static_cast<int>(Config::AppConfig::instance().getProcessingBackend()));
}

void OperationPipelineExecutor::init(
    std::vector<Operations::OperationDescriptor>&& operations,
    const Operations::OperationFactory& factory,
    const Common::ImageDim width,
    const Common::ImageDim height
)
{
    spdlog::debug("[OperationPipelineExecutor::init]: Initializing with {} operations.", operations.size());

    m_operations = std::move(operations);
    m_factory = &factory;

    if (!m_operations.empty()) {
        buildChain(width, height);
    } else {
        m_chain_built = false;
        m_pipeline = Halide::Pipeline();
        m_pipeline_params.clear();
    }
}

void OperationPipelineExecutor::updateRuntimeParams(std::vector<Operations::OperationDescriptor>&& operations)
{
    // Move the input vector into the member variable. This updates our internal state
    // with the latest values without any memory allocation or copying of the data structure.
    m_operations = std::move(operations);

    // FAST PATH: Iterate over the updated operations and sync the Halide Parameters.
    for (const auto& desc : m_operations)
    {
        if (!desc.enabled) continue;

        auto it { m_pipeline_params.find(desc.id) };
        if (it != m_pipeline_params.end())
        {
            // Parameter exists in cache, update its value
            if (auto val_res { desc.getParam<float>("value") }) {
                it->second.set(val_res.value());
            }
        } else {
            // If we reach here, the structure changed (new operation added) but init() wasn't called.
            // This indicates a logic error in the caller (should have called init instead).
            spdlog::warn("[OperationPipelineExecutor::updateRuntimeParams]: Param '{}' not found in cache. Structure changed? Call init().", desc.name);
        }
    }
}

Halide::Func OperationPipelineExecutor::applyOperations(const Halide::Func& input, const Halide::Var& x, const Halide::Var& y, const Halide::Var& c)
{
    Halide::Func current_stage("current_stage");
    current_stage(x, y, c) = input(x, y, c);

    m_pipeline_params.clear();

    for (const auto& desc : m_operations)
    {
        if (!desc.enabled) continue;

        if (!m_factory) {
            spdlog::error("[OperationPipelineExecutor::applyOperations]: Factory is null.");
            return current_stage;
        }

        auto op_impl_expected { m_factory->create(desc) };
        if (!op_impl_expected) continue;

        auto op_impl { std::move(op_impl_expected.value()) };
        auto* fusion_logic { dynamic_cast<const Operations::IOperationFusionLogic*>(op_impl.get()) };

        if (fusion_logic)
        {
            auto& param_ref { m_pipeline_params[desc.id] };
            if (auto val_res = desc.getParam<float>("value"))
            {
                param_ref.set(val_res.value());
                param_ref.set_estimate(val_res.value());
            }
            current_stage = fusion_logic->appendToFusedPipeline(current_stage, x, y, c, param_ref);
        }
    }
    return current_stage;
}

void OperationPipelineExecutor::ensurePluginLoaded(const std::string& plugin_name, const std::string& log_prefix)
{
    // One flag per plugin name: CPU and GPU executors may load different
    // plugins, and each must be loaded exactly once per process.
    static std::map<std::string, std::once_flag> flags;
    std::call_once(flags[plugin_name], [&plugin_name, &log_prefix] {
        try {
            Halide::load_plugin(plugin_name.c_str());
            spdlog::info("{}: {} plugin loaded.", log_prefix, plugin_name);
        } catch (const std::exception& e) {
            spdlog::warn("{}: load_plugin({}) failed ({}). "
                         "Expected if the autoscheduler is statically linked.",
                         log_prefix, plugin_name, e.what());
        }
    });
}

void OperationPipelineExecutor::buildChain(const Common::ImageDim width, const Common::ImageDim height)
{
    const AutoschedulerBackendConfig backend { backendConfig() };

    ensurePluginLoaded(backend.plugin_name, backend.log_prefix);

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

    // Autoscheduler estimates (input and output). Using the dimensions of the
    // currently loaded image: exact by construction.
    const int w { static_cast<int>(width) };
    const int h { static_cast<int>(height) };
    m_input.set_estimates({{0, w}, {0, h}, {0, 4}});
    final_output.output_buffer().set_estimates({{0, w}, {0, h}, {0, 4}});

    Halide::AutoschedulerParams params;
    params.name = backend.scheduler_name;
    for (const auto& [key, value] : backend.extra) {
        params.extra[key] = value;
    }

    try {
        m_pipeline = Halide::Pipeline(final_output);
        m_pipeline.apply_autoscheduler(backend.target, params);
        m_pipeline.compile_jit(backend.target);
        m_chain_built = true;
        spdlog::info("{}::buildOperationChain]: {} auto-scheduled and compiled "
                     "for {}x{} (target={}).",
                     backend.log_prefix, backend.scheduler_name, w, h,
                     backend.target.to_string());
    }
    catch (const Halide::CompileError& e) {
        spdlog::critical("{}::buildOperationChain]: CompileError: {}", backend.log_prefix, e.what());
        m_chain_built = false;
    }
    catch (const Halide::RuntimeError& e) {
        spdlog::critical("{}::buildOperationChain]: RuntimeError: {}", backend.log_prefix, e.what());
        m_chain_built = false;
    }
    catch (const std::exception& e) {
        spdlog::critical("{}::buildOperationChain]: Unexpected: {}", backend.log_prefix, e.what());
        m_chain_built = false;
    }
}

bool OperationPipelineExecutor::executeOnHalideBuffer(Halide::Buffer<float>& input_buffer, Halide::Buffer<float>& output_buffer)
{
    if (!m_chain_built || !m_pipeline.defined()) {
        return true;
    }
    try {
        // 1. Bind the actual C++ buffer memory to the Halide ImageParam.
        //    This is extremely fast (pointer copy), no data duplication.
        m_input.set(input_buffer);
        // 2. Execute the pipeline on the correct device.
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
