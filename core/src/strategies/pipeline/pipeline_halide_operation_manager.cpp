/**
 * @file pipeline_halide_operation_manager.cpp
 * @brief Implementation of PipelineHalideOperationManager.
 * @author CaptureMoment Team
 * @date 2026
 */

#include "strategies/pipeline/pipeline_halide_operation_manager.h"

#include <spdlog/spdlog.h>

namespace CaptureMoment::Core::Strategies {

PipelineHalideOperationManager::PipelineHalideOperationManager()
    : m_operation_factory(std::make_unique<Operations::OperationFactory>())
{
    // Register all available operations (Concrete Creators)
    Core::Operations::OperationRegistry::registerAll(*m_operation_factory);
    spdlog::debug("PipelineHalideOperationManager: Constructed with Pipeline and Worker contexts.");
    
    auto base_executor { Pipeline::PipelineBuilder::build(Pipeline::PipelineType::HalideOperation) };

    if (!base_executor) {
        spdlog::error("PipelineHalideOperationManager::PipelineHalideOperationManager: Builder returned null.");
        return;
    }

    // Downcasting to the concrete type. This is safe because we know the builder returns the correct type for this pipeline.
    auto* concrete_ptr { dynamic_cast<Pipeline::OperationPipelineExecutor*>(base_executor.get()) };
    
    if (!concrete_ptr) {
        spdlog::error("PipelineHalideOperationManager::PipelineHalideOperationManager: Critical type mismatch.");
        return;
    }

    // Release ownership from base_executor and transfer to m_executor
    base_executor.release();
    m_executor.reset(concrete_ptr);
}

void PipelineHalideOperationManager::init(std::vector<Operations::OperationDescriptor>&& operations, const Common::ImageDim width, const Common::ImageDim height)
{
    // 1. Detect structural changes (type, name, enabled) vs value changes (params)
    bool structure_changed { true };

    spdlog::debug("[PipelineHalideOperationManager::init]: m_last_operations.size() = {}, operations.size() = {}",
                  m_last_operations.size(), operations.size());

    if (m_last_operations.size() == operations.size()) {
        structure_changed = false;
        for (size_t i = 0; i < operations.size(); ++i)
        {

            // Compare type, name, enabled. Ignore params for this check.
            spdlog::debug("[PipelineHalideOperationManager::init]: Op[{}]: | last=({},{}) | new=({},{})",
                          operations[i].id,m_last_operations[i].name, m_last_operations[i].enabled, operations[i].name, operations[i].enabled);

            if (operations[i].id != m_last_operations[i].id ||
                operations[i].enabled != m_last_operations[i].enabled) {
                structure_changed = true;
                spdlog::debug("[PipelineHalideOperationManager::init]: Structure change detected at index {}", i);
                break;
            }
        }
    }
    spdlog::debug("[PipelineHalideOperationManager::init]: structure_changed = {}", structure_changed);

    // 2. Update the cache with the new operations (for future structural change detection)
    // This should be done before the lock to minimize time spent in critical section, as it's just a vector copy.
    // Note: We are moving the operations into the cache, which is fine because we will move it again into the executor if structure changed.
    m_last_operations = operations;

    // 3. Lock and update the executor based on whether the structure changed or not.
    std::lock_guard lock(m_mutex);

    if (structure_changed) {
        spdlog::info("[PipelineHalideOperationManager::init]: Structure changed. Recompiling pipeline.");
        // If the structure changed, we need to recompile the pipeline, which is more expensive. We call init() which will rebuild the graph and recompile.
        m_executor->init(std::move(operations), *m_operation_factory, width, height);
    } else {
        spdlog::trace("[PipelineHalideOperationManager::init]: Values only. Updating runtime params.");
        // If only values changed, we can skip recompilation and just update the parameters in the existing pipeline. This is the fast path.
        m_executor->updateRuntimeParams(std::move(operations));
    }
}

bool PipelineHalideOperationManager::execute(ImageProcessing::IWorkingImageHardware& working_image)
{
    std::lock_guard lock(m_mutex);

    if (!m_executor) {
        spdlog::debug("[PipelineHalideOperationManager::execute]: error call m_executor->execute");
        return false;
    }

    spdlog::debug("[PipelineHalideOperationManager::execute]: call m_executor->execute");
    return m_executor->execute(working_image);
}

} // namespace CaptureMoment::Core::Strategies
