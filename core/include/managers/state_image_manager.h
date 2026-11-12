/**
 * @file state_image_manager.h
 * @brief Declaration of StateImageManager class
 *
 * @details
 * This class manages the asynchronous processing state of the image being edited.
 * It acts as a coordinator between the image source, the infrastructure contexts
 * (Pipeline & Workers), and the working image memory.
 *
 * **Key Architectural Features:**
 * - **Contexts:** Owns `PipelineContext` (Strategies) and `WorkerContext` (Executors).
 * - **Move Semantics:** All data processing methods accept ownership of data via `std::move`.
 * - **Thread-Safe:** Uses mutexes to protect the working image and status flags.
 * - **Stateless Processing:** Does not store operation lists; it receives, processes, and discards.
 * - **Single Persistent Worker:** Uses ONE background thread for all processing, avoiding
 *   the overhead of thread creation/destruction during rapid UI interactions.
 * - **Coalescing Updates:** If an update is requested while the worker is busy,
 *   the new request overwrites the pending one. Intermediate requests are discarded,
 *   ensuring only the latest state is processed.
 *
 * @author CaptureMoment Team
 * @date 2026
 */

#pragma once

#include "operations/operation_descriptor.h"
#include "managers/i_source_manager.h"
#include "image_processing/interfaces/i_working_image_hardware.h"
#include "operations/operation_factory.h"
#include "common/types/image_types.h"

#include <vector>
#include <memory>
#include <mutex>
#include <future>
#include <string_view>
#include <string>
#include <optional>
#include <thread>
#include <atomic>
#include <condition_variable>


namespace CaptureMoment::Core {

namespace Pipeline {
class PipelineContext;
}

namespace Workers {
class WorkerContext;
}

namespace ImageProcessing {
class WorkingImageContext;
}


namespace Managers {


/**
 * @class StateImageManager
 * @brief Manages the working image lifecycle and coordinates asynchronous processing.
 *
 * @details
 * This class receives processing requests (e.g., apply operations) from the engine,
 * prepares the working memory, delegates execution to workers via the WorkerContext,
 * and updates the final result. It implements a coalescing strategy for incoming
 * operations to optimize responsiveness during rapid UI interactions.
 *
 * **Threading Model:**
 * The class owns a single persistent background thread (`m_worker_thread`).
 * No new threads are spawned during image processing.
 *
 * **Coalescing Behavior (e.g., Slider Dragging):**
 * - When `applyOperations` is called, if the worker is idle, processing starts immediately.
 * - If the worker is already processing, the new operations are stored as pending,
 *   **overwriting** any previously pending operations.
 * - Superseded `std::future` objects from intermediate calls are resolved immediately
 *   to prevent caller blocking.
 * - When the worker finishes its current task, it checks for pending work. If present,
 *   it processes the *latest* one. If absent, it goes to sleep.
 * - The final `std::future<bool>` returned by `applyOperations` resolves only when
 *   the operation *actually executed* completes.
 */
class StateImageManager {
public:
    /**
     * @brief Constructs a StateImageManager.
     *
     * @details
     * Initializes the PipelineContext and WorkerContext.
     *
     * @throws std::invalid_argument if source_manager is null.
     */
    explicit StateImageManager();

    /**
     * @brief Destructor.
     */
    ~StateImageManager();

    // Disable copy and assignment
    StateImageManager(const StateImageManager&) = delete;
    StateImageManager& operator=(const StateImageManager&) = delete;

    /**
     * @brief Applies a list of operations to the current image using move semantics.
     *
     * @details
     * This method is the primary entry point for image processing. It takes ownership
     * of the provided operation descriptors via `std::move` to avoid unnecessary copies.
     *
     * **Workflow:**
     * 1. If the worker thread is idle, the operations are dispatched immediately.
     * 2. If the worker thread is busy, the operations overwrite the pending queue.
     *    Any previous pending `std::future` is resolved instantly.
     * 3. The method returns immediately without blocking the UI thread.
     *
     * @param ops The list of operation descriptors to apply (moved into the method).
     * @return `std::future<bool>` representing the asynchronous result.
     *         The future resolves when the operation actually executed completes.
     */
    [[nodiscard]] std::future<bool> applyOperations(std::vector<Operations::OperationDescriptor>&& ops);

    /**
     * @brief Loads an image file into the internal SourceManager.
     * @param path Path to the image file.
     * @return true if successful.
     */
    [[nodiscard]] bool loadImage(std::string_view path);

    /**
     * @brief initOperations initializes the pipeline with a list of operations.
     * @param list of operations
     * @return true if successful.
     */
    [[nodiscard]] bool initOperations(std::vector<Operations::OperationDescriptor> &&ops);
    /**
     * @brief Commits the current working image (with applied operations) back to the SourceManager.
     *
     * @details
     * This effectively overwrites the original image data with the processed version.
     * Future resets or reloads will use this new "original" data.
     *
     * @return std::expected with void or error.
     */
    [[nodiscard]] std::expected<void, ErrorHandling::CoreError> commitWorkingImageToSource();

    /**
     * @brief Resets the image to its original state (synchronous).
     *
     * @details
     * This method applies an empty list of operations (reset) and waits for completion.
     * It is used by `PhotoEngine::loadImage` to ensure the image is ready immediately.
     *
     * @return `std::expected<void, CoreError>` indicating success or failure.
     */
    [[nodiscard]] std::expected<void, ErrorHandling::CoreError> resetToOriginal();

    /**
     * @brief Gets the path of the image source.
     *
     * @return A copy of the file path as `std::string`.
     */
    [[nodiscard]] std::string getImageSourcePath() const;

    /**
     * @brief Gets the width of the original source image.
     *
     * @details These methods query the SourceManager for the original image properties.
     * @return The width of the source image.
     */
    [[nodiscard]] Common::ImageDim getSourceWidth() const;

    /**
     * @brief Gets the height of the original source image.
     *
     * @details These methods query the SourceManager for the original image properties.
     * @return The height of the source image.
     */
    [[nodiscard]] Common::ImageDim getSourceHeight() const;

    /**
     * @brief Gets the number of channels of the original source image.
     *
     * @details These methods query the SourceManager for the original image properties.
     * @return The number of channels of the source image.
     */
    [[nodiscard]] Common::ImageChan getSourceChannels() const;

    /**
     * @brief
     * Exports the current working image data to CPU memory as an ImageRegion.
     */
    [[nodiscard]] std::expected<std::unique_ptr<Common::ImageRegion>, ErrorHandling::CoreError>
    getWorkingImageAsRegion() const;

    /**
     * @brief Gets a downsampled version of the working image for display.
     * Uses GPU-accelerated downsampling, much faster than exporting full image.
     *
     * @param target_width  Target display width.
     * @param target_height Target display height.
     * @return Downsampled ImageRegion on success, CoreError on failure.
     */
    [[nodiscard]] std::expected<std::unique_ptr<Common::ImageRegion>, ErrorHandling::CoreError>
    getDownsampledDisplayImage(Common::ImageDim target_width, Common::ImageDim target_height);

private:
    /**
     * @brief Main loop for the persistent worker thread.
     * @details Waits for new operations, processes them, and checks for coalesced
     *          operations. Runs continuously until @ref m_stop_requested is true.
     */
    void workerLoop();

    /**
     * @brief Executes the image processing pipeline synchronously.
     * @details Called from within the worker thread. Initializes the pipeline context
     *          and delegates execution to the Halide worker.
     * @param ops The operations to execute (moved).
     * @return true if the processing succeeded, false otherwise.
     */
    [[nodiscard]] bool executeProcessing(std::vector<Operations::OperationDescriptor> ops);

    /**
     * @brief Blocks until worker and pending queue are fully empty.
     */
    void waitForPendingProcessing();

    // ========================================================================
    // Worker Thread Synchronization
    // ========================================================================

    /**
     * @brief Atomic flag to signal the worker thread to exit cleanly.
     */
    std::atomic<bool> m_stop_requested{false};

    /**
     * @brief Mutex protecting access to @ref m_pending_work and @ref m_active_promise.
     */
    mutable std::mutex m_work_mutex;

    /**
     * @brief Condition variable to wake up the worker thread when new work is available.
     */
    std::condition_variable m_work_cv;

    // ========================================================================
    // State & Infrastructure
    // ========================================================================

    /**
     * @brief Mutex protecting access to `m_working_image` and `m_original_image_path`.
     */
    mutable std::mutex m_state_mutex;

    /**
     * @brief The Pipeline Context infrastructure.
     */
    std::unique_ptr<Pipeline::PipelineContext> m_pipeline_context;

    /**
     * @brief The Worker Context infrastructure.
     */
    std::unique_ptr<Workers::WorkerContext> m_worker_context;

    /**
     * @brief The current working image context.
     */
    std::unique_ptr<ImageProcessing::WorkingImageContext> m_working_image_context;

    /**
     * @brief File path of the original source image.
     */
    std::string m_original_image_path;

    /**
     * @brief Holds the latest operations requested while the worker is busy.
     * @details Overwritten on each new request to ensure only the most recent
     *          state is processed (coalescing). Middle requests are discarded.
     */
    std::optional<std::vector<Operations::OperationDescriptor>> m_pending_work;

    /**
     * @brief Shared pointer to the promise corresponding to the latest pending or active operation.
     * @details Using a shared_ptr allows safely resolving superseded futures
     *          from the caller thread without violating promise rules.
     */
    std::shared_ptr<std::promise<bool>> m_active_promise;

    /**
     * @brief Atomic flag indicating the system is completely idle.
     * @details True only when no work is currently processing AND no work is pending.
     *          Used by @ref waitForPendingProcessing() to know when it is safe to
     *          modify the underlying image source. Modified under @ref m_work_mutex
     *          to prevent race conditions.
     */
    std::atomic<bool> m_is_idle{true};

    /**
     * @brief Dependency to access original image tiles and metadata.
     */
    std::unique_ptr<Managers::ISourceManager> m_source_manager;

    /**
     * @brief The single persistent thread running @ref workerLoop.
     * @details Created at construction, joined at destruction. Avoids the overhead
     *          of spawning a new thread on every slider movement.
     */
    std::thread m_worker_thread;


    // ========================================================================
    // Idle State Synchronization
    // ========================================================================

    /**
     * @brief Mutex and CV dedicated to waiting for the idle state.
     * @details Decoupled from m_work_mutex to prevent deadlocks during shutdown.
     */
    std::mutex m_idle_mutex;

    /**
     * @brief  Condition variable to notify when the system becomes idle.
     */
    std::condition_variable m_idle_cv;
};

} // namespace Managers

} // namespace CaptureMoment::Core
