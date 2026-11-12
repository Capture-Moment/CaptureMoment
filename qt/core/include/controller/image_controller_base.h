/**
 * @file image_controller_base.h
 * @brief Qt bridge between Core (PhotoEngine) and Qt UI
 * @author CaptureMoment Team
 * @date 2025
 */

#pragma once

#include <QObject>
#include <memory>
#include <QThread>
#include <QUrl>

#include "engine/photo_engine.h"
#include "models/operations/i_operation_model.h"
#include "display/display_manager.h"
#include "managers/operation_state_manager.h"
#include "models/manager/operation_model_manager.h"

namespace CaptureMoment::UI {
class IOperationModel;

namespace Controller {
/**
 * @class ImageController
 * @brief Orchestrates Core processing and Qt UI updates
 *
 * Responsibilities:
 * - Owns PhotoEngine (Core)
 * - Manages worker thread for non-blocking operations
 * - Exposes Qt slots for QML/UI
 * - Emits Qt signals for UI updates
 * - Thread-safe image and operation handling
 */
class ImageControllerBase : public QObject {
    Q_OBJECT

    /** @property imageWidth The width of the currently loaded image. */
    Q_PROPERTY(int imageWidth READ imageWidth NOTIFY imageSizeChanged)
    /** @property imageHeight The height of the currently loaded image. */
    Q_PROPERTY(int imageHeight READ imageHeight NOTIFY imageSizeChanged)

    /**
     * @property displayManager
     * @brief Exposes the DisplayManager instance to QML.
     *
     * This property allows QML components to access the DisplayManager
     * for managing image display, zoom, pan, and viewport size.
     */
    Q_PROPERTY(CaptureMoment::UI::Display::DisplayManager* displayManager READ displayManager CONSTANT)

    /**
     * @property operationStateManager
     * @brief Exposes the OperationStateManager instance to QML.
     *
     * This property allows QML components or operation models to access the OperationStateManager
     * for managing the cumulative state of active operations.
     */
    Q_PROPERTY(CaptureMoment::UI::Managers::OperationStateManager* operationStateManager READ operationStateManager CONSTANT)

public:
    /**
     * @brief Constructs ImageController
     * @param parent Parent QObject
     */
    explicit ImageControllerBase(QObject* parent = nullptr);

    /**
     * @brief Destructor - cleans up worker thread
     */
    ~ImageControllerBase();

    /**
     * @brief Initializes the operation state manager with the current operation models.
     *
     * This method retrieves the descriptors from all registered operation models
     * and initializes the OperationStateManager with them. It should be called
     * after all models have been created and registered.
     */
    void initializeOperations();

    /**
     * @brief Get current image width
     */
    [[nodiscard]] int imageWidth() const noexcept { return m_image_width; }

    /**
     * @brief Get current image height
     */
    [[nodiscard]] int imageHeight() const noexcept { return m_image_height; }

    /**
     * @brief Register an operation model for notifications
     *
     * Called by IOperationModel::setImageController()
     * The model will receive operationCompleted/operationFailed signals
     * after operations are processed.
     *
     * @param model Pointer to IOperationModel
     */
    void registerModel(IOperationModel* model);

    /**
     * @brief Get current displayManager instance
     */
    [[nodiscard]] CaptureMoment::UI::Display::DisplayManager* displayManager() { return m_display_manager.get(); }

    /**
     * @brief Get current operationStateManager instance
     */
    [[nodiscard]] CaptureMoment::UI::Managers::OperationStateManager* operationStateManager() { return m_operation_state_manager.get(); }

    /**
     * @brief Get current operationModelManager instance
     */
    CaptureMoment::UI::Models::Manager::OperationModelManager* operationModelManager() { return m_operation_model_manager.get(); }

    /**
     * @brief Perform actual image load (runs on worker thread).
     * This method contains the common logic for loading and updating the display.
     * @param file_path Path to the image file to load.
     */
    void doLoadImage(const QString& file_path);

    /**
     * @brief Perform actual operations (runs on worker thread).
     * @details
     * This method contains the common logic for applying operations and updating the display.
     * Takes ownership of the operations to allow zero-copy transfer to the engine.
     * @param operations Vector of operation descriptors (moved)
     */
    void doApplyOperations(std::vector<Core::Operations::OperationDescriptor>&& operations);

    /**
     * @brief Loads an image from a QUrl (typically from QML FileDialog).
     * Converts the QUrl to a native file path and initiates the loading process.
     * @param file_url The QUrl representing the image file path.
     */
    Q_INVOKABLE void loadImageFromUrl(const QUrl& file_url);

private :
    /**
     * @brief Worker thread for non-blocking operations
     */
    QThread m_worker_thread;


    /**
     * @brief Holds the current source image for display operations.
     *
     * This member stores a shared pointer to the ImageRegion that represents
     * the current state of the image being processed and displayed.
     * It is updated whenever a new image is loaded or when operations
     * are applied that result in a new working image.
     * The shared_ptr ensures safe access from multiple threads and
     * automatic memory management.
     */
    std::shared_ptr<Core::Common::ImageRegion> m_source_image;

    /**
     * @brief Registered operation models for notifications
     * These models will receive operationCompleted/operationFailed signals
     */
    std::vector<IOperationModel*> m_registered_models;

    /**
     * @brief The unique operation state manager instance for managing cumulative operations.
     */
    std::unique_ptr<CaptureMoment::UI::Managers::OperationStateManager> m_operation_state_manager {nullptr};


    /**
     * @brief The unique operation model manager instance for creating and managing operation models.
     */
    std::unique_ptr<CaptureMoment::UI::Models::Manager::OperationModelManager> m_operation_model_manager {nullptr};

public slots:
    /**
     * @brief Load image from file path (non-blocking)
     * @param file_path Path to image file
     */
    void loadImage(const QString& file_path);

    /**
     * @brief Apply operation with parameters (non-blocking)
     * Note: This slot is designed to be called from QML or other UI components when the user changes an operation parameter (e.g., moves a slider).
     * 1. It takes a vector of OperationDescriptors representing the current state of all active operations.
     * 2. The vector is passed by value to ensure compatibility with Qt's signal/slot system and to allow for move semantics.
     * 3. The method then schedules the actual processing on the worker thread to avoid blocking the UI.
     *
     * @param operations Vector of operation descriptors
     */
    void applyOperations(std::vector<Core::Operations::OperationDescriptor> operations);

    /**
     * @brief Internal: Handle load image result
     * @param success Whether load succeeded
     * @param error_msg Error message if failed
     */
    void onImageLoadResult(bool success, const QString& error_msg);

    /**
     * @brief Internal: Handle operation result
     * @param success Whether operation succeeded
     * @param error_msg Error message if failed
     */
    void onOperationResult(bool success, const QString& error_msg);

signals:
    /**
     * @brief Emitted when image loaded successfully
     * @param width Image width
     * @param height Image height
     */
    void imageLoaded(int width, int height);

    /**
     * @brief Emitted when image load fails
     * @param error Error message
     */
    void imageLoadFailed(QString error);

    /**
     * @brief Emitted when operation completes
     */
    void operationCompleted();

    /**
     * @brief Emitted when operation fails
     * @param error Error message
     */
    void operationFailed(QString error);

    /**
     * @brief Emitted when image size changes
     */
    void imageSizeChanged();

protected:
    /**
     * @brief Core processing engine
     */
    std::shared_ptr<Core::Engine::PhotoEngine> m_engine;

    /**
     * @brief The unique displaymanager instance for managing display updates
     */
    std::unique_ptr<CaptureMoment::UI::Display::DisplayManager> m_display_manager {nullptr};

    /**
     * @brief Connects the operationRequested signal from all created models to the OperationStateManager.
     * This method should be called after models are created.
     */
    void connectModelsToStateManager();

    /**
     * @brief Current image width
     */
    int m_image_width{0};

    /**
     * @brief Current image height
     */
    int m_image_height{0};
};

} // namespace Controller

} // namespace CaptureMoment::UI
