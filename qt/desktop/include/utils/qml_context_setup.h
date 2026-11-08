/**
 * @file qml_context_setup.h
 * @brief Manages creation and setup of all C++ objects for QML
 * @author CaptureMoment Team
 * @date 2025
 */

#pragma once

#include <QQmlContext>
#include <memory>

namespace CaptureMoment::UI {

namespace Controller {
class ImageControllerBase;
}

namespace Serializer {
class SerializerController;
}

/**
 * @brief Central manager for QML context setup
 *
 * Responsibilities:
 * - Creates and manages core objects (controller)
 * - Delegates operation model management (creation, registration, connection) to ImageControllerBase
 * - Registers core objects to QML context
 * - Lifetime management via shared_ptr
 * - Ensures objects are correctly initialized before use
 */
class QmlContextSetup {
public:
    /**
     * @brief Setup complete QML context with all objects
     *
     * Creates:
     * - ImageController (main orchestrator)
     *
     * Registers to QML:
     * - "controller" → ImageController (which internally manages models)
     *
     * @param context The QML context to setup
     * @return true if setup was successful, false otherwise.
     */
    static bool setupContext(QQmlContext* context);

    /**
     * @brief Releases the objects created by setupContext().
     *
     * Must be called before main() returns, while spdlog and the Qt application
     * are still alive. The static members below are otherwise destroyed by
     * __cxa_finalize() during exit(), at which point spdlog's registry is
     * already gone and the controller destructors - which log, and join a
     * QThread - crash.
     */
    static void shutdown();

    /**
     * @brief Sets the SerializerController instance to be registered with the QML context.
     * This must be called before setupContext.
     * @param ui_serializer_controller A unique pointer to the SerializerController instance.
     */
    static void setSerializerController(std::unique_ptr<Serializer::SerializerController> ui_serializer_controller);

    /**
     * @brief Registers the SerializerController (if set) to the QML context.
     * @param context Pointer to the QQmlContext.
     * @return true on success, false otherwise.
     */
    static bool registerSerializerToQml(QQmlContext* context);

    /**
     * @brief Getter for the main ImageControllerBase instance.
     * @return A shared pointer to the ImageControllerBase instance.
     */
    static std::shared_ptr<Controller::ImageControllerBase> getControllerMainScene() { return m_controller_main_scene; }

    /**
     * @brief Getter for the UISerializerManager instance.
     * @return A pointer to the SerializerController instance.
     */
    static Serializer::SerializerController* getSerializerController() { return m_serializer_controller.get(); }

private:
    /**
     * @brief Static shared pointer to the main ImageControllerBase instance.
     * This singleton-like pattern holds the central controller object which orchestrates
     * the image processing pipeline, manages operation models, and handles communication
     * between the UI layer and the core processing engine.
     * It is created by createControllerMainScene() and registered to the QML context.
     */
    static std::shared_ptr<Controller::ImageControllerBase> m_controller_main_scene;

    /**
     * @brief Static unique pointer to the SerializerController instance.
     * This object handles UI-specific serialization tasks and is registered to the QML context
     * if provided via setSerializerController().
     */
    static std::unique_ptr<Serializer::SerializerController> m_serializer_controller;

    /**
     * @brief Create the central ImageController orchestrator.
     * @return true if the controller was created successfully, false otherwise.
     */
    [[nodiscard]] static bool createControllerMainScene();

    /**
     * @brief Register core objects (like controller) to QML context
     * @param context Pointer to the QQmlContext.
     * @return true on success, false otherwise.
     */
    [[nodiscard]] static bool registerCoreToQml(QQmlContext* context);
};

} // namespace CaptureMoment::UI
