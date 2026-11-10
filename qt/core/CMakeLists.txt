# qt/core/CMakeLists.txt

project(CaptureMomentQtCore LANGUAGES CXX)

message(STATUS "Configuring CaptureMoment Qt Core Library...")

set(CAPTUREMOMENT_QT_CORE_LIB capturemoment_qt_core_lib)

# --- Creation of the library ---
qt_add_library(${CAPTUREMOMENT_QT_CORE_LIB} STATIC)

capturemoment_enable_optimizations(${CAPTUREMOMENT_QT_CORE_LIB})

# Explicitly ensure AUTOMOC runs on this target
set_target_properties(${CAPTUREMOMENT_QT_CORE_LIB} PROPERTIES
    AUTOMOC TRUE
    AUTORCC TRUE
    AUTOUIC TRUE
)

qt_add_shaders(${CAPTUREMOMENT_QT_CORE_LIB}
    PREFIX /shaders
    FILES
        shaders/glsl/image_display.vert
        shaders/glsl/image_display.frag
)

set(CORE_SOURCES
    # Controller
    src/controller/image_controller_base.cpp
    src/controller/image_controller_painted.cpp
    src/controller/image_controller_sgs.cpp
    src/controller/image_controller_rhi.cpp

    # Serializer
    src/serializer/serializer_controller.cpp
    
    # Manager
    src/managers/operation_state_manager.cpp

    # Display
    src/display/display_manager.cpp
    src/display/viewport_manager.cpp

    # Rendering
    src/rendering/rhi_image_item.cpp
    src/rendering/rhi_image_item_renderer.cpp
    src/rendering/sgs_image_item.cpp
    src/rendering/painted_image_item.cpp
    src/rendering/base_image_item.cpp

    # Operations
    src/models/manager/operation_model_manager.cpp
    src/models/operations/base_adjustment_model.cpp

    # Operations base adjustment
    src/models/operations/base_adjustment_models/brightness_model.cpp
    src/models/operations/base_adjustment_models/contrast_model.cpp
    src/models/operations/base_adjustment_models/highlights_model.cpp
    src/models/operations/base_adjustment_models/shadows_model.cpp
    src/models/operations/base_adjustment_models/whites_model.cpp
    src/models/operations/base_adjustment_models/blacks_model.cpp
)

set(QOBJECT_HEADERS
    # Rendering
    include/rendering/i_rendering_item_base.h
    include/rendering/rhi_image_item.h
    include/rendering/rhi_image_item_renderer.h
    include/rendering/sgs_image_item.h
    include/rendering/painted_image_item.h
    include/rendering/base_image_item.h

    # Display
    include/display/display_manager.h
    include/display/viewport_manager.h
    include/display/viewport_config.h

    # Controller
    include/controller/image_controller_base.h
    include/controller/image_controller_painted.h
    include/controller/image_controller_sgs.h
    include/controller/image_controller_rhi.h

    # Serializer
    include/serializer/serializer_controller.h

    # Manager
    include/managers/operation_state_manager.h

    # Operations
    include/domain/operation_parameters.h
    include/models/operations/i_operation_model.h
    include/models/operations/operation_provider.h
    include/models/manager/operation_model_manager.h

    # List Operations base adjustment
    include/models/operations/base_adjustment_model.h
    include/models/operations/basic_adjustment_models/brightness_model.h
    include/models/operations/basic_adjustment_models/contrast_model.h
    include/models/operations/basic_adjustment_models/highlights_model.h
    include/models/operations/basic_adjustment_models/shadows_model.h
    include/models/operations/basic_adjustment_models/whites_model.h
    include/models/operations/basic_adjustment_models/blacks_model.h
)

target_sources(${CAPTUREMOMENT_QT_CORE_LIB}
    PRIVATE
        ${CORE_SOURCES} 
        ${QOBJECT_HEADERS}

    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
            # Controller
            controller/image_controller_base.h
            controller/image_controller_painted.h
            controller/image_controller_sgs.h
            controller/image_controller_rhi.h

            # Serializer
            serializer/serializer_controller.h

            # Manager
            managers/operation_state_manager.h

            # Display
            display/display_manager.h
            display/viewport_manager.h
            display/viewport_config.h

            # Rendering
            rendering/i_rendering_item_base.h
            rendering/base_image_item.h
            rendering/rhi_image_item.h
            rendering/rhi_image_item_renderer.h
            rendering/sgs_image_item.h
            rendering/painted_image_item.h

            # Operations
            domain/operation_parameters.h
            models/operations/i_operation_model.h
            models/operations/operation_provider.h
            models/manager/operation_model_manager.h

            # List Operations base adjustment
            models/operations/base_adjustment_model.h
            models/operations/base_adjustment_models/brightness_model.h
            models/operations/base_adjustment_models/contrast_model.h
            models/operations/base_adjustment_models/highlights_model.h
            models/operations/base_adjustment_models/shadows_model.h
            models/operations/base_adjustment_models/whites_model.h
            models/operations/base_adjustment_models/blacks_model.h
)

# --- Configuration---
target_link_libraries(${CAPTUREMOMENT_QT_CORE_LIB}
    PUBLIC
        Qt6::Core
        Qt6::Quick
        Qt6::Gui
        Qt6::GuiPrivate
        Qt6::ShaderTools
        capturemoment_core        
)

target_include_directories(${CAPTUREMOMENT_QT_CORE_LIB}
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include> 
        $<INSTALL_INTERFACE:include> 
    PRIVATE
    # If necessary, include other internal paths to this library
    # ${CMAKE_CURRENT_SOURCE_DIR}/src # If the .cpp files need to be included from src/
    # Path to the core headers if needed in the .cpp files of this library
    # "${CMAKE_SOURCE_DIR}/core/include"
)

message(STATUS "CaptureMoment Qt Core Library configured successfully.")
message(STATUS "   - Target: ${CAPTUREMOMENT_QT_CORE_LIB}")
message(STATUS "")
