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
        buildOperationChain(width, height);
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

        auto it = m_pipeline_params.find(desc.id);
        if (it != m_pipeline_params.end())
        {
            // Parameter exists in cache, update its value
            if (auto val_res = desc.getParam<float>("value")) {
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

} // namespace CaptureMoment::Core::Pipeline
