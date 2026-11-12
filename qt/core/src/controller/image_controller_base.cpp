/**
 * @file image_controller_base.cpp
 * @brief Implementation of ImageControllerBase
 * @date 2025
 */

#include "controller/image_controller_base.h"

#include "models/operations/i_operation_model.h"
#include "display/display_manager.h"
#include "managers/operation_state_manager.h"
#include "models/operations/base_adjustment_model.h"
#include "common/error_handling/core_error.h"
#include "common/types/image_types.h"

#include <QMetaObject>
#include <algorithm>

#include <spdlog/spdlog.h>

namespace CaptureMoment::UI::Controller {

ImageControllerBase::ImageControllerBase(QObject* parent)
    : QObject(parent)
{
    spdlog::debug("[ImageControllerBase::ImageControllerBase]: Constructing ImageControllerBase");

    m_operation_state_manager = std::make_unique<Managers::OperationStateManager>();
    spdlog::info("[ImageControllerBase::ImageControllerBase]: Initialized OperationStateManager");

    m_operation_model_manager = std::make_unique<Models::Manager::OperationModelManager>();
    spdlog::info("[ImageControllerBase::ImageControllerBase]: Initialized OperationModelManager");

    if (!m_operation_model_manager->createBasicAdjustmentModels())
    {
        spdlog::critical("[ImageControllerBase::ImageControllerBase]: Failed to create basic adjustment models.");
        throw std::runtime_error("[ImageControllerBase::ImageControllerBase]: Critical failure during model creation.");
    }

    initializeOperations();

    m_display_manager = std::make_unique<Display::DisplayManager>(this);
    spdlog::info("[ImageControllerBase::ImageControllerBase]: Initialized DisplayManager");

    m_engine = std::make_shared<Core::Engine::PhotoEngine>();
    spdlog::info("[ImageControllerBase::ImageControllerBase]: Initialized PhotoEngine");

    connectModelsToStateManager();
    spdlog::debug("[ImageControllerBase::ImageControllerBase]: Completed construction");
}

ImageControllerBase::~ImageControllerBase()
{
    spdlog::debug("[ImageControllerBase::~ImageControllerBase]: Destroying ImageControllerBase");
    m_worker_thread.quit();
    m_worker_thread.wait();
    spdlog::debug("[ImageControllerBase::~ImageControllerBase]: Worker thread stopped and destroyed");
}

void ImageControllerBase::registerModel(IOperationModel* model)
{
    if (!model) {
        spdlog::warn("[ImageControllerBase::registerModel]: Attempting to register nullptr");
        return;
    }

    if (std::find(m_registered_models.begin(), m_registered_models.end(), model) != m_registered_models.end()) {
        spdlog::debug("[ImageControllerBase::registerModel]: Model already registered");
        return;
    }

    m_registered_models.push_back(model);
    spdlog::debug("[ImageControllerBase::registerModel]: Model registered. Total models: {}", m_registered_models.size());
}

void ImageControllerBase::loadImage(const QString& file_path)
{
    if (file_path.isEmpty()) {
        spdlog::warn("[ImageControllerBase::loadImage]: Empty file path provided");
        emit imageLoadFailed("Empty file path");
        return;
    }

    spdlog::info("[ImageControllerBase::loadImage]: Calling method-thread doLoadImage() Loading {}", file_path.toStdString());

    // Run on worker thread to avoid blocking UI
    QMetaObject::invokeMethod(this, [this, file_path]() {
        doLoadImage(file_path);
    }, Qt::QueuedConnection);
}

void ImageControllerBase::loadImageFromUrl(const QUrl& file_url)
{
    if (file_url.isEmpty()) {
        spdlog::warn("[ImageControllerBase::loadImageFromUrl]: Empty file URL received");
        emit imageLoadFailed("Empty file URL");
        return;
    }

    QString native_path { file_url.toLocalFile() };

    if (native_path.isEmpty()) {
        spdlog::warn("[ImageControllerBase::loadImageFromUrl]: Failed to convert URL to local file path: {}", file_url.toString().toStdString());

        if (!file_url.isLocalFile()) {
            spdlog::warn("[ImageControllerBase::loadImageFromUrl]: Selected URL is not a local file: {}", file_url.toString().toStdString());
            emit imageLoadFailed("Selected URL is not a local file");
            return;
        } else {
            spdlog::warn("[ImageControllerBase::loadImageFromUrl]: Failed to convert URL to local file path: {}", file_url.toString().toStdString());
            emit imageLoadFailed("Failed to convert URL to local file path");
            return;
        }
    }

    spdlog::info("[ImageControllerBase::loadImageFromUrl]: Converted URL to native path: {}", native_path.toStdString());

    loadImage(native_path);
}

void ImageControllerBase::applyOperations(std::vector<Core::Operations::OperationDescriptor> operations)
{
    if (!m_engine)
    {
        spdlog::warn("[ImageControllerBase::applyOperations]: Engine not available");
        emit operationFailed("No image loaded");
        return;
    }

    if (operations.empty())
    {
        spdlog::warn("[ImageControllerBase::applyOperations]: Empty operation list provided");
        emit operationFailed("No operations specified");
        return;
    }

    spdlog::info("[ImageControllerBase::applyOperations]: Applying {} operation(s)", operations.size());

    if (m_operation_state_manager)
    {
   QMetaObject::invokeMethod(this, [this, ops = std::move(operations)]() mutable {
        doApplyOperations(std::move(ops));
    }, Qt::QueuedConnection);
    } else {
        spdlog::error("[ImageControllerBase::applyOperations]: OperationStateManager is null during legacy applyOperations call!");
        emit operationFailed("Internal error: OperationStateManager not initialized");
    }
}

void ImageControllerBase::doLoadImage(const QString& file_path)
{
    spdlog::info("[ImageControllerBase::doLoadImage]: Starting load on worker thread");

    // 1. Load image
    auto load_result { m_engine->loadImage(file_path.toStdString()) };

    if (!load_result) {
        spdlog::error("[ImageControllerBase::doLoadImage]: Load failed for {}", file_path.toStdString());
        auto err { load_result.error() };
        QString error_msg { QString::fromStdString(
            std::format("CoreError [{}]: {}", static_cast<int>(err), Core::ErrorHandling::to_string(err))
        )};
        onImageLoadResult(false, error_msg);
        return;
    }

    // 2. Get Metadata
    m_image_width = m_engine->width();
    m_image_height = m_engine->height();

    spdlog::info("[ImageControllerBase::doLoadImage]: Image loaded {}x{}", m_image_width, m_image_height);

    // 3. Notify DisplayManager of source size
    if (m_display_manager) {
        m_display_manager->setSourceImageSize(
            static_cast<int>(m_image_width),
            static_cast<int>(m_image_height)
        );
    }

    // 4. Get display size from DisplayManager
    QSize downsample_size { m_display_manager->downsampleSize() };

    if (downsample_size.isEmpty()) {
        spdlog::error("[ImageControllerBase::doLoadImage]: Invalid downsample size");
        onImageLoadResult(false, "Invalid downsample size");
        return;
    }

    // 5. Get downsampled image directly (GPU → small ImageRegion)
    spdlog::debug("[ImageControllerBase::doLoadImage]: Requesting downsampled image {}x{}",
                  downsample_size.width(), downsample_size.height());

    auto display_image_result { m_engine->getDownsampledDisplayImage(
        static_cast<Core::Common::ImageDim>(downsample_size.width()),
        static_cast<Core::Common::ImageDim>(downsample_size.height())
    )};

    if (!display_image_result) {
        spdlog::error("[ImageControllerBase::doLoadImage]: Failed to get downsampled image: {}",
                      Core::ErrorHandling::to_string(display_image_result.error()));
        onImageLoadResult(false, "Failed to get display image");
        return;
    }

    spdlog::debug("[ImageControllerBase::doLoadImage]: Got downsampled image successfully");

    // 6. Update DisplayManager with const& (no copy, no shared_ptr)
    if (m_display_manager) {
        m_display_manager->createDisplayImage(std::move(display_image_result.value()));
        spdlog::info("[ImageControllerBase::doLoadImage]: DisplayManager updated");
    } else {
        spdlog::error("[ImageControllerBase::doLoadImage]: No DisplayManager available!");
    }

    onImageLoadResult(true, "");
}

void ImageControllerBase::doApplyOperations(std::vector<Core::Operations::OperationDescriptor>&& operations)
{
    spdlog::debug("[ImageControllerBase::doApplyOperations]: Starting operation processing with {} operations", operations.size());

    if (!m_engine) {
        spdlog::error("[ImageControllerBase::doApplyOperations]: No engine available");
        onOperationResult(false, "No engine available");
        return;
    }

    // 1. Trigger Core Processing and wait for completion
    // The core uses a deferred future: the continuation that updates the working image
    // and clears the "update in progress" flag only runs when .get() is called.
    spdlog::debug("[ImageControllerBase::doApplyOperations]: Applying operations via PhotoEngine");
    auto apply_future { m_engine->applyOperations(std::move(operations)) };
    if (!apply_future.valid()) {
        onOperationResult(false, "Failed to start operation");
        return;
    }
    bool apply_ok { apply_future.get() };
    if (!apply_ok) {
        onOperationResult(false, "Operation processing failed");
        return;
    }

    // 2. Get display size from DisplayManager
    QSize downsample_size { m_display_manager->downsampleSize() };

    if (downsample_size.isEmpty()) {
        spdlog::error("[ImageControllerBase::doApplyOperations]: Invalid downsample size");
        onOperationResult(false, "Invalid downsample size");
        return;
    }

    // 3. Get downsampled image directly (GPU → small ImageRegion)
    spdlog::debug("[ImageControllerBase::doApplyOperations]: Requesting downsampled image {}x{}", 
                  downsample_size.width(), downsample_size.height());

    auto display_image_result { m_engine->getDownsampledDisplayImage(
        static_cast<Core::Common::ImageDim>(downsample_size.width()),
        static_cast<Core::Common::ImageDim>(downsample_size.height())
    )};

    if (!display_image_result) {
        spdlog::error("[ImageControllerBase::doApplyOperations]: Failed to get downsampled display image: {}",
                      Core::ErrorHandling::to_string(display_image_result.error()));
        onOperationResult(false, "Failed to get display image");
        return;
    }

    spdlog::debug("[ImageControllerBase::doApplyOperations]: Got downsampled image successfully");

    // 4. Update DisplayManager with const& (no copy, no shared_ptr)
    if (m_display_manager) {
        m_display_manager->createDisplayImage(std::move(display_image_result.value()));
        spdlog::info("[ImageControllerBase::doApplyOperations]: Display updated");
    } else {
        spdlog::warn("[ImageControllerBase::doApplyOperations]: No DisplayManager");
    }

    onOperationResult(true, "");
}

void ImageControllerBase::onImageLoadResult(bool success, const QString& error_msg)
{
    spdlog::debug("[ImageControllerBase::onImageLoadResult]: success={}", success);

    if (success) {
        spdlog::info("[ImageControllerBase::onImageLoadResult]: Image loaded successfully ({}x{})",
                     m_image_width, m_image_height);
        emit imageSizeChanged();
        emit imageLoaded(m_image_width, m_image_height);
    } else {
        spdlog::error("[ImageControllerBase::onImageLoadResult]: Image load failed - {}", error_msg.toStdString());
        emit imageLoadFailed(error_msg);
    }
}

void ImageControllerBase::onOperationResult(bool success, const QString& error_msg)
{
    spdlog::debug("[ImageControllerBase::onOperationResult]: success={}", success);

    if (success) {
        spdlog::info("[ImageControllerBase::onOperationResult]: Operation completed successfully");
        emit operationCompleted();
    } else {
        spdlog::error("[ImageControllerBase::onOperationResult]: Operation failed - {}", error_msg.toStdString());
        emit operationFailed(error_msg);
    }
}

void ImageControllerBase::initializeOperations()
{
    const auto& models { m_operation_model_manager->getBaseAdjustmentModels() };

    std::vector<Core::Operations::OperationDescriptor> descriptors;
    descriptors.reserve(models.size());

    for (const auto& model : models) {
        if (model) {
            descriptors.push_back(model->getDescriptor());
        }
    }

    m_operation_state_manager->initialize(descriptors);
    spdlog::info("[ImageControllerBase::initializeOperations]: {} operations initialized (all enabled, neutral values).",
                 descriptors.size());
}

void ImageControllerBase::connectModelsToStateManager()
{
    spdlog::debug("[ImageControllerBase::connectModelsToStateManager]: Starting model connections");

    if (!m_operation_model_manager || !m_operation_state_manager) {
        spdlog::critical("[ImageControllerBase::connectModelsToStateManager]: ModelManager or StateManager is null. Cannot proceed.");
        throw std::runtime_error("ImageControllerBase: Critical failure during model-to-state-manager connection setup.");
    }

    // Use the specific list of BaseAdjustmentModel (SAFE, NO CAST NEEDED)
    const auto& models { m_operation_model_manager->getBaseAdjustmentModels() };
    for (const auto& model : models) { // model is std::shared_ptr<UI::Models::Operations::BaseAdjustmentModel>
        if (model) {
            // Connect the EXISTING valueChanged signal from BaseAdjustmentModel
            // The type is known at compile time, no cast needed here.
            QObject::connect(model.get(), &UI::Models::Operations::BaseAdjustmentModel::valueChanged,
                             [this, model /* Capture the shared_ptr to the specific model */ ](float new_value) {
                                 spdlog::debug("[ImageControllerBase::connectModelsToStateManager]: Value changed signal received for model {}", model->name().toStdString());

                                 // This lambda is called when the specific BaseAdjustmentModel's value changes.

                                 // 1. Update the OperationStateManager with the CURRENT state of this specific model
                                 if (m_operation_state_manager) {
                                     auto descriptor { model->getDescriptor() }; // Get the descriptor for the specific model that changed
                                     m_operation_state_manager->addOrUpdateOperation(descriptor);
                                     spdlog::debug("[ImageControllerBase::connectModelsToStateManager]: Operation '{}' updated in StateManager via valueChanged signal.", descriptor.name);
                                 } else {
                                     spdlog::warn("[ImageControllerBase::connectModelsToStateManager]: Received valueChanged signal, but StateManager is null.");
                                     return; // Exit if StateManager is not available
                                 }

                                 // 2. Retrieve the full list of active operations from OperationStateManager
                                auto active_ops { m_operation_state_manager->getActiveOperations() };

                                 // 3. Trigger the applyOperations workflow with the full list of active operations (Move semantics)
                                applyOperations(std::move(active_ops));


                             });
            spdlog::debug("[ImageControllerBase::connectModelsToStateManager]: Connected model {}", model->name().toStdString());
        }
    }
    spdlog::info("[ImageControllerBase::connectModelsToStateManager]: All {} BaseAdjustment models connected to StateManager via valueChanged signal.", models.size());
}

} // namespace CaptureMoment::UI::Controller
