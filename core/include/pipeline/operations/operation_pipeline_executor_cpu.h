/**
 * @file operation_pipeline_executor_cpu.h
 * @brief Declaration of OperationPipelineExecutorCPU (Fused Adjustment Pipeline).
 *
 * @details
 * This class is a concrete implementation of `OperationPipelineExecutor` optimized for CPU execution.
 * All the shared pipeline logic (graph, autoscheduling, compilation, execution) lives in
 * `OperationPipelineExecutor`; this class only supplies the Adams2019 backend configuration
 * and the CPU-specific working image access path.
 *
 * @author CaptureMoment Team
 * @date 2026
 */

#pragma once

#include "pipeline/operations/operation_pipeline_executor.h"

namespace CaptureMoment::Core {

namespace Pipeline {

/**
 * @class OperationPipelineExecutorCPU
 * @brief Concrete executor for fused Halide adjustment pipelines. CPU
 *
 * @details
 * This executor chains multiple image adjustment operations (Brightness, Contrast, etc.)
 * into a single Halide graph. It relies on the `IHalidePipelineExecutor` base class
 * to provide the standardized 4-channel input parameter (`m_input`).
 */
class OperationPipelineExecutorCPU final : public OperationPipelineExecutor {
public:
    /**
     * @brief Destructor.
     */
    ~OperationPipelineExecutorCPU() override = default;

    [[nodiscard]] bool execute(ImageProcessing::IWorkingImageHardware& working_image) override;

protected:
    [[nodiscard]] AutoschedulerBackendConfig backendConfig() const override;
};
} // namespace Pipeline
} // namespace CaptureMoment::Core
