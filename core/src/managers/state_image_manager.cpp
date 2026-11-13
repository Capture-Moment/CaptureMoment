/**
 * @file state_image_manager.cpp
 * @brief Implementation of StateImageManager
 * @author CaptureMoment Team
 * @date 2026
 */

#include "managers/state_image_manager.h"

#include "pipeline/pipeline_context.h"
#include "workers/worker_context.h"
#include "image_processing/working_image_context.h"
#include "managers/source_manager.h"

#include <spdlog/spdlog.h>
#include <utility>

namespace CaptureMoment::Core::Managers {

StateImageManager::StateImageManager()
    : m_pipeline_context(std::make_unique<Pipeline::PipelineContext>())
    , m_worker_context(std::make_unique<Workers::WorkerContext>())
    , m_working_image_context(std::make_unique<ImageProcessing::WorkingImageContext>())
    , m_source_manager(std::make_unique<Managers::SourceManager>())
{
    if (!m_source_manager) {
        spdlog::critical("[StateImageManager::StateImageManager]: Null dependency provided during construction.");
        throw std::invalid_argument("[StateImageManager::StateImageManager]: Null dependency provided.");
    }

    // Start the worker thread ONLY AFTER all members are constructed and validation passes.
    m_worker_thread = std::thread(&StateImageManager::workerLoop, this);
}

StateImageManager::~StateImageManager()
{
    // Signal the worker thread to stop and wake it up safely (prevents lost wakeup)
    {
        std::scoped_lock<std::mutex> lock(m_work_mutex);
        m_stop_requested.store(true, std::memory_order_release);
    }
    m_work_cv.notify_one();

    // Wait for the thread to finish its current task and exit
    if (m_worker_thread.joinable()) {
        m_worker_thread.join();
    }

    spdlog::debug("[StateImageManager::~StateImageManager]: Destroyed.");
}

// ============================================================
// State Management
// ============================================================

bool StateImageManager::loadImage(std::string_view path)
{
     // 1. Wait for any pending processing to complete
    waitForPendingProcessing();

    // 2. Load the file into the INTERNAL SourceManager
    auto load_result { m_source_manager->loadFile(path) };

    if (!load_result) {
        spdlog::error("[StateImageManager::loadImage]: Failed to load file '{}': {}", path, static_cast<int>(load_result.error()));
        return false;
    }

    const Common::ImageDim w { m_source_manager->width() };
    const Common::ImageDim h { m_source_manager->height() };
    auto tile { m_source_manager->getTile(0, 0, w, h) };

    if (!tile) {
        spdlog::error("[StateImageManager::loadImage]: Failed to get tile for loaded image '{}': {}", path, static_cast<int>(tile.error()));
        return false;
    }

    {
        std::scoped_lock lock(m_state_mutex);
        if (!m_working_image_context->prepare(std::move(tile.value()))) {
            spdlog::error("[StateImageManager::loadImage]: prepare failed for '{}'", path);
            return false;
        }
        m_original_image_path = std::string(path);
    }

    // 3. Clear any pending operations from previous image
    {
        std::scoped_lock lock(m_work_mutex);
        m_pending_work.reset();
    }

    spdlog::info("[StateImageManager::loadImage]: Image '{}' loaded successfully ({}x{}).",
                 path, m_source_manager->width(), m_source_manager->height());

    return true;
}

bool StateImageManager::initOperations(std::vector<Operations::OperationDescriptor> &&ops)
{
    // Wait for any running processing before rebuilding the pipeline
    waitForPendingProcessing();

    const Common::ImageDim width  { m_source_manager->width() };
    const Common::ImageDim height { m_source_manager->height() };

    if (width == 0 || height == 0) {
        spdlog::error("[StateImageManager::initOperations]: No image loaded.");
        return false;
    }

    try {
        auto& halide_manager { m_pipeline_context->getHalideManager() };

        // Builds the pipeline for (width, height) and this structure.
        // Only the structure and size matter here; values are set later.
        halide_manager.init(std::move(ops), width, height);

        spdlog::info("[StateImageManager::initOperations]: Pipeline built for {}x{}.", width, height);
        return true;
    }
    catch (const std::exception& e) {
        spdlog::error("[StateImageManager::initOperations]: Exception: {}", e.what());
        return false;
    }
}

void StateImageManager::workerLoop()
{
    spdlog::trace("[StateImageManager::workerLoop]: Thread started.");


    while (!m_stop_requested.load(std::memory_order_acquire))
    {
        std::optional<std::vector<Operations::OperationDescriptor>> work_to_do;
        std::shared_ptr<std::promise<bool>> work_promise;

        // --------------------------------------------------------
        // PHASE 1: Wait for and acquire work
        // --------------------------------------------------------
        {
            std::unique_lock<std::mutex> lock(m_work_mutex);

            // Wait until: stop requested OR work available
            m_work_cv.wait(lock, [this] {
                return m_stop_requested.load(std::memory_order_acquire)
                    || m_pending_work.has_value();
            });

            if (m_stop_requested.load(std::memory_order_acquire))
            {
                // Resolve any pending promise before exit
                if (m_active_promise) {
                    try { m_active_promise->set_value(false); }
                    catch (const std::future_error&) {}
                }

                // CRITICAL: Clear pending state and signal idle to prevent waiters from hanging
                m_pending_work.reset();
                m_is_idle.store(true, std::memory_order_release);
                m_idle_cv.notify_all();

                break;
            }

            // Acquire work (move out of pending)
            work_to_do = std::move(m_pending_work);
            m_pending_work.reset();
            work_promise = m_active_promise;
        }

        // --------------------------------------------------------
        // PHASE 2: Execute processing
        // --------------------------------------------------------
        const bool success { executeProcessing(std::move(*work_to_do)) };

        // --------------------------------------------------------
        // PHASE 3: Check for coalesced work
        // --------------------------------------------------------
        bool has_more_work { false };
        {
            std::scoped_lock<std::mutex> lock(m_work_mutex);
            has_more_work = m_pending_work.has_value();

            if (!has_more_work)
            {
                // No more work - this is the final operation in the chain
                // Set idle to true UNDER the mutex to prevent race conditions
                m_is_idle.store(true, std::memory_order_release);

                // Resolve the promise
                if (work_promise) {
                    try { work_promise->set_value(success); }
                    catch (const std::future_error&) {}
                }
            }
            // If has_more_work: keep m_is_idle = false, loop back immediately
        }
    }

    spdlog::trace("[StateImageManager::workerLoop]: Thread exiting.");
}

bool StateImageManager::executeProcessing(std::vector<Operations::OperationDescriptor> ops)
{
    try
    {
        spdlog::trace("[StateImageManager::executeProcessing]: Executing {} operations.", ops.size());

        auto& halide_manager { m_pipeline_context->getHalideManager() };

        const Common::ImageDim width { m_source_manager->width() };
        const Common::ImageDim height { m_source_manager->height() };

        halide_manager.init(std::move(ops), width, height);

        auto worker { m_worker_context->getHalideOperationWorker() };

        std::shared_ptr<ImageProcessing::IWorkingImageHardware> working_image;
        {
            std::scoped_lock<std::mutex> lock(m_state_mutex);
            working_image = m_working_image_context->getWorkingImage();
        }

        if (!working_image) {
            spdlog::error("[StateImageManager::executeProcessing]: No working image available.");
            return false;
        }

        auto future { worker.execute(*m_pipeline_context, *working_image) };
        return future.get();
    }
    catch (const std::exception& e)
    {
        spdlog::error("[StateImageManager::executeProcessing]: Exception during processing: {}", e.what());
        return false;
    }
}

std::expected<void, ErrorHandling::CoreError> StateImageManager::commitWorkingImageToSource()
{
    // Ensure no processing is running before committing
    waitForPendingProcessing();

    std::shared_ptr<ImageProcessing::IWorkingImageHardware> working_image_hw;
    {
        std::scoped_lock lock(m_state_mutex);
        working_image_hw = m_working_image_context->getWorkingImage();
    }

    if (!working_image_hw) {
        spdlog::error("[StateImageManager::commitWorkingImageToSource]: No working image available.");
        return std::unexpected(ErrorHandling::CoreError::InvalidWorkingImage);
    }

    auto cpu_copy_result { working_image_hw->getFullResImage() };
    if (!cpu_copy_result) {
        spdlog::error("[StateImageManager::commitWorkingImageToSource]: CPU export failed: {}",
                       ErrorHandling::to_string(cpu_copy_result.error()));
        return std::unexpected(cpu_copy_result.error());
    }

    std::unique_ptr<Common::ImageRegion> cpu_copy { std::move(cpu_copy_result.value()) };

    if (!m_source_manager->setTile(*cpu_copy)) {
        spdlog::error("[StateImageManager::commitWorkingImageToSource]: Write to source failed.");
        return std::unexpected(ErrorHandling::CoreError::IOError);
    }

    spdlog::info("[StateImageManager::commitWorkingImageToSource]: Changes committed to source.");
    return {};
}

std::expected<void, ErrorHandling::CoreError> StateImageManager::resetToOriginal()
{
    // 1. Trigger the async processing with an empty list of operations
    auto future { applyOperations(std::vector<Operations::OperationDescriptor>{}) };

    // 2. Block and wait for the operation to complete
    // This allows the caller (PhotoEngine) to assume the image is ready when this returns.
    bool success { future.get() };

    // 3. Convert boolean result to std::expected
    if (!success) {
        spdlog::error("[StateImageManager::resetToOriginal]: Processing failed.");
        return std::unexpected(ErrorHandling::CoreError::AllocationFailed);
    }

    return {};
}

std::string StateImageManager::getImageSourcePath() const
{
   return m_source_manager->getImageSourcePath();
}

std::future<bool> StateImageManager::applyOperations(std::vector<Operations::OperationDescriptor>&& ops)
{
    spdlog::trace("[StateImageManager::applyOperations]: Received {} ops.", ops.size());

    auto promise { std::make_shared<std::promise<bool>>() };
    auto future { promise->get_future() };

    {
        std::scoped_lock<std::mutex> lock(m_work_mutex);

        if (m_active_promise) {
            try { m_active_promise->set_value(true); } // Superseded = success (not an error)
            catch (const std::future_error&) {}
        }

        // Store new promise and work
        m_active_promise = promise;
        m_pending_work = std::move(ops);

        // CRITICAL: Set idle to false UNDER the mutex
        m_is_idle.store(false, std::memory_order_release);
    }

    // Wake the persistent worker thread
    m_work_cv.notify_one();

    return future;
}

void StateImageManager::waitForPendingProcessing()
{
    std::unique_lock<std::mutex> lock(m_idle_mutex);
    m_idle_cv.wait(lock, [this] {
        return m_is_idle.load(std::memory_order_acquire);
    });
}

Common::ImageDim StateImageManager::getSourceWidth() const
{
    std::scoped_lock lock(m_state_mutex);
    return m_source_manager ? m_source_manager->width() : 0;
}

Common::ImageDim StateImageManager::getSourceHeight() const
{
    std::scoped_lock lock(m_state_mutex);
    return m_source_manager ? m_source_manager->height() : 0;
}

Common::ImageChan StateImageManager::getSourceChannels() const
{
    std::scoped_lock lock(m_state_mutex);
    return m_source_manager ? m_source_manager->channels() : 0;
}

std::expected<std::unique_ptr<Common::ImageRegion>, ErrorHandling::CoreError> StateImageManager::getWorkingImageAsRegion() const
{
    std::scoped_lock lock(m_state_mutex);

    if (!m_working_image_context) {
        return std::unexpected(ErrorHandling::CoreError::InvalidWorkingImage);
    }

    return m_working_image_context->getWorkingImageAsRegion();
}

std::expected<std::unique_ptr<Common::ImageRegion>, ErrorHandling::CoreError>
StateImageManager::getDownsampledDisplayImage(Common::ImageDim target_width, Common::ImageDim target_height)
{
    std::scoped_lock lock(m_state_mutex);

    if (!m_working_image_context) {
        spdlog::error("[StateImageManager::getDownsampledDisplayImage]: No WorkingImageContext");
        return std::unexpected(ErrorHandling::CoreError::InvalidWorkingImage);
    }

    return m_working_image_context->getDownsampled(target_width, target_height);
}

} // namespace CaptureMoment::Core::Managers
