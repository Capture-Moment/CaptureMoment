# qt/desktop/CMakeLists.txt

project(CaptureMomentUIDesktop LANGUAGES CXX)

message(STATUS "Configuring CaptureMoment Desktop UI...")

find_package(Qt6 REQUIRED COMPONENTS Widgets)
message(STATUS "   - Qt6 Widget found")

set(CAPTUREMOMENT_DESKTOP_TARGET capturemoment_desktop)

qt_add_executable(${CAPTUREMOMENT_DESKTOP_TARGET})

set_target_properties(${CAPTUREMOMENT_DESKTOP_TARGET} PROPERTIES
    AUTOMOC TRUE
    AUTORCC TRUE
    AUTOUIC TRUE
)


set(QOBJECT_HEADERS
    # Utils
    include/utils/qml_context_setup.h
    include/utils/splash_screen.h

    # Rendering QML
    include/rendering/qml_rhi_image_item.h
    include/rendering/qml_sgs_image_item.h
    include/rendering/qml_painted_image_item.h    
)

set(CORE_SOURCES
       src/main.cpp

       # Utils
       src/utils/qml_context_setup.cpp
       src/utils/splash_screen.cpp

       # Rendering QML
       src/rendering/qml_rhi_image_item.cpp
       src/rendering/qml_sgs_image_item.cpp
       src/rendering/qml_painted_image_item.cpp
)

set(CORE_HEADERS
        # Utils
        /utils/qml_context_setup.h
        /utils/splash_screen.h

        # Rendering QML
        /rendering/qml_rhi_image_item.h
        /rendering/qml_sgs_image_item.h
        /rendering/qml_painted_image_item.h
)

set (QML_FILE
    # main
    qml/CaptureMoment/DesktopMain.qml

    # Views
    qml/CaptureMoment/views/EditorView.qml

    # Layout
    qml/CaptureMoment/layout/PanelManager.qml
    qml/CaptureMoment/layout/LeftPanel.qml
    qml/CaptureMoment/layout/CenterPanel.qml
    qml/CaptureMoment/layout/RightPanel.qml
    qml/CaptureMoment/layout/BottomPanel.qml
    qml/CaptureMoment/layout/TopPanel.qml

    # Controls
    qml/CaptureMoment/controls/DropdownControl.qml
    qml/CaptureMoment/controls/SliderControl.qml

    # Operations basic adjustment
    qml/CaptureMoment/operations/BrightnessOperation.qml
    qml/CaptureMoment/operations/ContrastOperation.qml
    qml/CaptureMoment/operations/HighlightsOperation.qml
    qml/CaptureMoment/operations/ShadowsOperation.qml
    qml/CaptureMoment/operations/WhitesOperation.qml
    qml/CaptureMoment/operations/BlacksOperation.qml

    # Panels
    qml/CaptureMoment/panels/OperationsView.qml
    qml/CaptureMoment/panels/TonePanel.qml
    qml/CaptureMoment/panels/base/CollapsiblePanel.qml
    qml/CaptureMoment/panels/base/PanelHeader.qml

    # Menu
    qml/CaptureMoment/app/AppMenuBar.qml

    # Display
    qml/CaptureMoment/display/DisplayArea.qml
    
    # Styles
    qml/CaptureMoment/styles/Icons.qml
)

# ====================================================================
# Icon configuration for different platforms
# ====================================================================
# --- WINDOWS ---
if(WIN32)
    set(APP_ICON_RC "${CMAKE_CURRENT_BINARY_DIR}/app_icon.rc")
    set(ICON_PATH_WIN "${CMAKE_SOURCE_DIR}/assets/icons/favicon.ico")
    file(WRITE "${APP_ICON_RC}" "IDI_ICON1 ICON DISCARDABLE \"${ICON_PATH_WIN}\"")
    list(APPEND CORE_SOURCES "${APP_ICON_RC}")
endif()

# --- MACOS ---
if(APPLE)
    set(APP_ICON_MACOS "${CMAKE_SOURCE_DIR}/assets/icons/favicon.icns")
    set_source_files_properties(${APP_ICON_MACOS} PROPERTIES
        MACOSX_PACKAGE_LOCATION "Resources"
    )
    list(APPEND CORE_SOURCES "${APP_ICON_MACOS}")
endif()

# ====================================================================
# End
# ====================================================================

# ====================================================================
# Resources configuration
# ====================================================================
qt_add_resources(${CAPTUREMOMENT_DESKTOP_TARGET} "config"
    PREFIX /
    FILES
       qtquickcontrols2.conf
)

qt_add_resources(${CAPTUREMOMENT_DESKTOP_TARGET} "image-splash"
    PREFIX "splash"
    BASE "${CMAKE_SOURCE_DIR}/assets/desktop/splash"
    FILES
        "${CMAKE_SOURCE_DIR}/assets/desktop/splash/splash-screen.png"
)

qt_add_resources(${CAPTUREMOMENT_DESKTOP_TARGET} "app-icon"
    PREFIX "icons"
    BASE "${CMAKE_SOURCE_DIR}/assets/icons"
    FILES
         "${CMAKE_SOURCE_DIR}/assets/icons/favicon.png"
)

# ====================================================================
# End
# ====================================================================

qt_add_qml_module(${CAPTUREMOMENT_DESKTOP_TARGET}
    URI CaptureMoment.desktop
    VERSION 1.0
    QML_FILES
       ${QML_FILE}
    RESOURCES
       ${ASSET_FILES}
)

target_sources(${CAPTUREMOMENT_DESKTOP_TARGET}
    PRIVATE
       ${CORE_SOURCES}
       ${QOBJECT_HEADERS}

    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
            ${CORE_HEADERS}
)


# =====================================================================
# Platform-specific implementation (compile-time selection)
# =====================================================================
if(WIN32)
    set(PLATFORM_IMPL src/platform/desktop_platform_windows.cpp)
elseif(APPLE)
    set(PLATFORM_IMPL src/platform/desktop_platform_macos.cpp)
elseif(UNIX)                                      # ← elseif, pas if
    set(PLATFORM_IMPL src/platform/desktop_platform_linux.cpp)
else()
    message(FATAL_ERROR "Unsupported platform: ${CMAKE_SYSTEM_NAME}")
endif()

target_sources(${CAPTUREMOMENT_DESKTOP_TARGET} PRIVATE ${PLATFORM_IMPL})

# =====================================================================
# Bundle properties for macOS and iOS
# ====================================================================

# Autogenerated bundle properties for macOS and iOS.
# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
set_target_properties(${CAPTUREMOMENT_DESKTOP_TARGET} PROPERTIES
    OUTPUT_NAME "CaptureMoment"
    MACOSX_BUNDLE_GUI_IDENTIFIER org.capturerepo.desktop
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR} #.${PROJECT_VERSION_MINOR} is not working
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

install(TARGETS capturemoment_desktop
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(WIN32)
    install(DIRECTORY $<TARGET_FILE_DIR:${CAPTUREMOMENT_DESKTOP_TARGET}>/
            DESTINATION ${CMAKE_INSTALL_BINDIR}
            FILES_MATCHING PATTERN "*.dll")
    set(deploy_tool_options_arg "--no-system-d3d-compiler")
endif()

if(UNIX AND NOT APPLE)
    set_target_properties(${CAPTUREMOMENT_DESKTOP_TARGET} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    )
    set(deploy_tool_options_arg "")
endif()

if(APPLE)
    set_target_properties(${CAPTUREMOMENT_DESKTOP_TARGET} PROPERTIES
        MACOSX_BUNDLE_ICON_FILE "favicon.icns"
    )
    set(deploy_tool_options_arg "")
endif()


qt_generate_deploy_qml_app_script(
    TARGET ${CAPTUREMOMENT_DESKTOP_TARGET}
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
    DEPLOY_USER_QML_MODULES_ON_UNSUPPORTED_PLATFORM
    NO_TRANSLATIONS
    DEPLOY_TOOL_OPTIONS ${deploy_tool_options_arg}
)
install(SCRIPT ${deploy_script})

# =====================================================================
# End
# ====================================================================

target_link_libraries(${CAPTUREMOMENT_DESKTOP_TARGET}
    PRIVATE
        capturemoment_qt_core_lib
        Qt6::Widgets
)

include(GNUInstallDirs)

if(WIN32)
    add_custom_command(TARGET ${CAPTUREMOMENT_DESKTOP_TARGET} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_RUNTIME_DLLS:${CAPTUREMOMENT_DESKTOP_TARGET}>
                $<TARGET_FILE_DIR:${CAPTUREMOMENT_DESKTOP_TARGET}>
        COMMAND_EXPAND_LISTS)
endif()

# =====================================================================
# Installation of the PNG for the Linux application menu (standard hicolor)
# =====================================================================
if(UNIX AND NOT APPLE)
    install(FILES "${CMAKE_SOURCE_DIR}/assets/icons/favicon.png"
            DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/256x256/apps"
            RENAME "capturemoment.png"
    )
endif()
# =====================================================================
# End
# =====================================================================

message(STATUS "CaptureMoment Desktop UI configured successfully.")
message(STATUS "   - Target: ${CAPTUREMOMENT_DESKTOP_TARGET}")
message(STATUS "   - QML URI: CaptureMoment.desktop")
message(STATUS "")
