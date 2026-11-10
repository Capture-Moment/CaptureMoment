/**
 * @file pipeline_halide_operation_manager.h
 * @brief Concrete manager for Halide-based adjustment operations.
 *
 * @details
 * This class implements `IPipelineManager` to handle standard pixel adjustments
 *
 * @author CaptureMoment Team
 * @date 2026
 */

#pragma once

#include "strategies/pipeline/interfaces/i_pipeline_manager.h"
#include "pipeline/operations/operation_pipeline_executor.h"
#include "operations/operation_factory.h"
#include "operations/operation_descriptor.h"
#include "common/types/memory_type.h"

#include <vector>
#include <memory>
#include <mutex>

namespace CaptureMoment::Core {
namespace Strategies {

/**
 * @class PipelineHalideOperationManager
 * @brief Thread-safe manager responsible for fused Halide operation pipelines.
 */
class PipelineHalideOperationManager final : public IPipelineManager {
public:
    /**
     * @brief Constructor with Dependency Injection.
     */
    explicit PipelineHalideOperationManager();

    virtual ~PipelineHalideOperationManager() = default;

    // Disable copy/move (Manager owns a mutex and pointers, copying is unsafe/complex)
    PipelineHalideOperationManager(const PipelineHalideOperationManager&) = delete;
    PipelineHalideOperationManager& operator=(const PipelineHalideOperationManager&) = delete;
    PipelineHalideOperationManager(PipelineHalideOperationManager&&) = delete;
    PipelineHalideOperationManager& operator=(PipelineHalideOperationManager&&) = delete;

    /**
     * @brief Executes the Halide pipeline on the image (Thread-Safe).
     *
     * @param working_image The image to process.
     * @return true if execution succeeded.
     */
    [[nodiscard]] virtual bool execute(ImageProcessing::IWorkingImageHardware& working_image) override;

    /**
     * @brief Initializes the manager with a new list of operations (Thread-Safe).
     *
     * @details
     * Uses the stored builder to create a new executor and swaps it atomically.
     *
     * @param operations The list of operation descriptors (moved).
     * @param width The width of the input image.
     * @param height The height of the input image.
     */
    void init(std::vector<Operations::OperationDescriptor>&& operations, const Common::ImageDim width, const Common::ImageDim height);

private:
    /**
     * @brief Mutex to protect m_executor during concurrent init() and execute().
     */
    mutable std::mutex m_mutex;

    /**
     * @brief Pointer to the concrete Halide executor.
     * @details
     * Access must be guarded by m_mutex.
     */
    std::unique_ptr<Pipeline::OperationPipelineExecutor> m_executor;

    /**
     * @brief Cache of the previous operation list to detect structural changes.
     */
    std::vector<Operations::OperationDescriptor> m_last_operations;

    /**
     * @brief Factory for creating concrete operation instances.
     */
    std::unique_ptr<Operations::OperationFactory> m_operation_factory;
};

} // namespace Strategies
} // namespace CaptureMoment::Core
