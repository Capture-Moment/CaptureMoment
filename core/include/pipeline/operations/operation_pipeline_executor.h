/**
 * @file operation_pipeline_executor.h
 * @brief Declaration of OperationPipelineExecutor (Fused Adjustment Pipeline).
 *
 * @details
 * This class is a concrete implementation of `IPipelineExecutor` and `IHalidePipelineExecutor`.
 * It implements the "Fused Pipeline" strategy for image adjustments using Halide.
 *
 * **Architecture Strategy:**
 * - **Inheritance**: Inherits `m_input` (4-channel Float32) from `IHalidePipelineExecutor`.
 * - **Single Compilation**: Uses `Halide::Pipeline` object to store the compiled JIT code.
 *   The compilation happens once in `init()` when operations change, not during execution.
 * - **Zero-Copy**: Execution binds the user's buffer to the inherited `m_input` and runs the pipeline.
 *
 * @author CaptureMoment Team
 * @date 2026
 */

#pragma once

#include "pipeline/interfaces/i_pipeline_executor.h"
#include "pipeline/interfaces/i_halide_pipeline_executor.h"
#include "operations/operation_descriptor.h"
#include "operations/operation_factory.h"
#include "common/types/memory_type.h"

#include <vector>
#include <unordered_map>

namespace CaptureMoment::Core {

namespace Pipeline {

/**
 * @class OperationPipelineExecutor
 * @brief Concrete executor for fused Halide adjustment pipelines.
 *
 * @details
 * This executor chains multiple image adjustment operations (Brightness, Contrast, etc.)
 * into a single Halide graph. It relies on the `IHalidePipelineExecutor` base class
 */
class OperationPipelineExecutor : public IPipelineExecutor, public IHalidePipelineExecutor {
public:
    /**
     * @brief Default Constructor.
     *
     * @details
     * Initializes the backend configuration (CPU/GPU) from AppConfig.
     * The `m_input` member is initialized by the base class.
     */
    OperationPipelineExecutor();

    /**
     * @brief Destructor.
     */
    virtual ~OperationPipelineExecutor() = default;

    /**
     * @brief Updates the list of operations and rebuilds the graph.
     * @details
     * Legacy method wrapper around `init`.
     */
    void updatePipeline(std::vector<Operations::OperationDescriptor> operations);

    /**
     * @brief Initializes the executor with a moved list of operations (Optimized).
     *
     * @details
     * This method is the main entry point for building and compiling a new Halide pipeline.
     * It takes ownership of the operations vector (Move Semantics) and stores it in `m_operations`.
     * It then calls `buildOperationChain()` to construct the Halide graph and `applyScheduling()`
     * to optimize it, before finally compiling it into `m_pipeline`.
     *
     * @param operations The list of operation descriptors.
     * @param factory The operation factory reference.
     * @param width The width of the input image.
     * @param height The height of the input image.
     */
    void init(
        std::vector<Operations::OperationDescriptor>&& operations,
        const Operations::OperationFactory& factory,
        const Common::ImageDim width,
        const Common::ImageDim height
    );

    /**
     * @brief Updates the runtime values of the cached parameters WITHOUT recompiling.
     *
     * @details
     * This is the key method for interactive performance (slider movement).
     * It takes ownership of the provided operations (Move Semantics) to update
     * the internal state `m_operations` and sync the `Halide::Param` objects.
     * IMPORTANT: The structure of operations (names, order) must match the
     * structure used during the last `init()` call.
     *
     * @param operations The list of operation descriptors with updated values (Moved).
     */
    void updateRuntimeParams(std::vector<Operations::OperationDescriptor>&& operations);

protected: 
     /**
     * @brief Applies scheduling directives (Vectorization/Parallelism/GPU tiling).
     * @details  Called during the build phase to optimize `output_func`
     * @param pipeline 
     * @param x 
     * @param y
     * @param c
     */
    virtual void applyScheduling(Halide::Func& pipeline, Halide::Var& x, Halide::Var& y, Halide::Var& c) const = 0;
    
    /**
     * @brief Applies all operations to the input function.
     * @details  Chains the operations together to form a complete Halide function graph.
     * @param input The input function.
     * @param x The x variable.
     * @param y The y variable.
     * @param c The c variable.
     * @return The resulting Halide function.
     */
    [[nodiscard]] Halide::Func applyOperations(const Halide::Func& input, const Halide::Var& x, const Halide::Var& y, const Halide::Var& c);

    /**
     * @brief Builds the Halide function graph based on `m_operations`.
     * @details
     * Iterates through operations, creates concrete instances, and chains them
     * using `m_input` as the source.
     * @param width The width of the input image.
     * @param height The height of the input image.
     */
    virtual void buildOperationChain(const Common::ImageDim width, const Common::ImageDim height) = 0;

    /**
     * @brief The compiled Halide pipeline object.
     * @details
     * Storing this allows us to execute the pipeline repeatedly without recompiling.
     */
    Halide::Pipeline m_pipeline;

    /**
     * @brief Flag indicating if the pipeline has been successfully built and compiled.
     */
    bool m_chain_built{false};

private:
    /**
     * @brief Stores the list of operations to be fused.
     */
    std::vector<Operations::OperationDescriptor> m_operations;

    /**
     * @brief Pointer to the operation factory.
     * @details
     * Non-owning pointer. Stored to allow dynamic creation of operation objects
     * during the graph building phase.
     */
    const Operations::OperationFactory* m_factory;

    /**
     * @brief Cache of dynamic parameters for the current pipeline.
     * @details
     * Key: Operation id
     * Value: The Halide::Param<float> object used in the compiled graph.
     */
    std::unordered_map<uint64_t, Halide::Param<float>> m_pipeline_params;
};
} // namespace Pipeline
} // namespace CaptureMoment::Core
