#include <QApplication>
#include <QQuickWindow>
#include <QQmlApplicationEngine>

#include <platform/desktop_platform.h>

#include "utils/qml_context_setup.h"
#include "utils/splash_screen.h"

#include "rendering/qml_painted_image_item.h"
#include "rendering/qml_sgs_image_item.h"
#include "rendering/qml_rhi_image_item.h"

#include "core_initialization.h"

#include <spdlog/spdlog.h>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    CaptureMoment::UI::Platform::applyApplicationSettings();

    CaptureMoment::UI::SplashScreen splash;
    splash.show();
    app.processEvents();

    spdlog::info("Initialization");
    splash.showMessage("Initializing core...");
    app.processEvents();
    CaptureMoment::Core::initialize();

    splash.showMessage("Registering components...");
    app.processEvents();

    // Scoped so the QML engine - and every QML object still holding a pointer
    // to the controller - is destroyed before QmlContextSetup::shutdown() runs.
    const int exit_code = [&]() -> int {
        QQmlApplicationEngine engine;

        // Register QML types Rendering
        qmlRegisterType<CaptureMoment::UI::QMLPaintedImageItem>(
            "CaptureMoment.UI.Rendering.Painted", 1, 0, "QMLPaintedImageItem"
        );
        qmlRegisterType<CaptureMoment::UI::QMLSGSImageItem>(
            "CaptureMoment.UI.Rendering.SGS", 1, 0, "QMLSGSImageItem"
            );
        qmlRegisterType<CaptureMoment::UI::QMLRHIImageItem>(
            "CaptureMoment.UI.Rendering.RHI", 1, 0, "QMLRHIImageItem"
            );

        splash.showMessage("Setting up context...");
        app.processEvents();

        // Setup QML context once
        auto context = engine.rootContext();
        if (!CaptureMoment::UI::QmlContextSetup::setupContext(context)) {
            spdlog::error("Failed to setup QML context");
            return -1;
        }

        // Handle QML engine errors
        QObject::connect(
            &engine,
            &QQmlApplicationEngine::objectCreationFailed,
            &app,
            []() { QCoreApplication::exit(-1); },
            Qt::QueuedConnection
        );

        // Load the main QML module
        splash.showMessage("Loading UI...");
        app.processEvents();
        engine.loadFromModule("CaptureMoment.desktop", "DesktopMain");

        if (engine.rootObjects().isEmpty()) {
            spdlog::error("Failed to load QML module");
            return -1;
        }

        QObject* rootObject { engine.rootObjects().first() };
        if (auto* window { qobject_cast<QQuickWindow*>(rootObject) })
        {
            QObject::connect(window, &QQuickWindow::frameSwapped, window, [window, &splash]() {
                splash.close();
                window->raise();
            }, Qt::SingleShotConnection);
        } else {
            splash.close();
        }

        return app.exec();
    }();

    // Release the statics owned by QmlContextSetup while spdlog and the
    // QApplication are still alive. Left to static destruction at exit(), the
    // controller destructors log through an already-destroyed spdlog registry
    // and join a QThread after QCoreApplication is gone - a segfault on quit.
    CaptureMoment::UI::QmlContextSetup::shutdown();

    return exit_code;
}
