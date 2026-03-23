cmake_minimum_required(VERSION 3.21)
project(BabylonNativePlayground VERSION 0.1.0)

add_subdirectory(Dependencies)

add_executable(BabylonNativePlayground
    Source/main.cpp
    Source/SceneInspector.h
    Source/SceneInspector.cpp
    Source/PlaygroundPanel.h
    Source/PlaygroundPanel.cpp)

target_link_libraries(BabylonNativePlayground
    PRIVATE AppRuntime
    PRIVATE GraphicsDevice
    PRIVATE NativeEngine
    PRIVATE NativeInput
    PRIVATE NativeOptimizations
    PRIVATE Console
    PRIVATE Window
    PRIVATE ScriptLoader
    PRIVATE XMLHttpRequest
    PRIVATE Canvas
    PRIVATE SDL3::SDL3
    PRIVATE imgui
    ${ADDITIONAL_LIBRARIES}
    ${BABYLON_NATIVE_PLAYGROUND_EXTENSION_LIBRARIES})

target_compile_features(BabylonNativePlayground PRIVATE cxx_std_20)
target_include_directories(BabylonNativePlayground PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Source)

source_group("Source" FILES
    Source/main.cpp
    Source/SceneInspector.h
    Source/SceneInspector.cpp
    Source/PlaygroundPanel.h
    Source/PlaygroundPanel.cpp)

# Copy Babylon.js scripts to build output
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/node_modules/babylonjs/babylon.max.js")
    set(BABYLON_SCRIPTS
        "node_modules/babylonjs/babylon.max.js"
        "node_modules/babylonjs-loaders/babylonjs.loaders.js"
        "node_modules/babylonjs-materials/babylonjs.materials.js")

    set(APP_SCRIPTS
        "Scripts/playground.js"
        "Scripts/Loaders/loader_glb.js"
        "Scripts/Loaders/loader_obj.js"
        "Scripts/Loaders/loader_env.js")

    target_sources(BabylonNativePlayground PRIVATE ${BABYLON_SCRIPTS} ${APP_SCRIPTS})

    foreach(SCRIPT ${BABYLON_SCRIPTS})
        get_filename_component(SCRIPT_NAME "${SCRIPT}" NAME)
        add_custom_command(
            OUTPUT "${CMAKE_CFG_INTDIR}/Scripts/${SCRIPT_NAME}"
            COMMAND "${CMAKE_COMMAND}" -E copy "${CMAKE_CURRENT_SOURCE_DIR}/${SCRIPT}" "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/Scripts/${SCRIPT_NAME}"
            COMMENT "Copying ${SCRIPT_NAME}"
            MAIN_DEPENDENCY "${CMAKE_CURRENT_SOURCE_DIR}/${SCRIPT}")
    endforeach()

    foreach(SCRIPT ${APP_SCRIPTS})
        # Preserve subdirectory structure (e.g. Scripts/Loaders/loader_glb.js)
        get_filename_component(SCRIPT_DIR "${SCRIPT}" DIRECTORY)
        get_filename_component(SCRIPT_NAME "${SCRIPT}" NAME)
        add_custom_command(
            OUTPUT "${CMAKE_CFG_INTDIR}/${SCRIPT_DIR}/${SCRIPT_NAME}"
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/${SCRIPT_DIR}"
            COMMAND "${CMAKE_COMMAND}" -E copy "${CMAKE_CURRENT_SOURCE_DIR}/${SCRIPT}" "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/${SCRIPT_DIR}/${SCRIPT_NAME}"
            COMMENT "Copying ${SCRIPT_DIR}/${SCRIPT_NAME}"
            MAIN_DEPENDENCY "${CMAKE_CURRENT_SOURCE_DIR}/${SCRIPT}")
    endforeach()

    source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${BABYLON_SCRIPTS} ${APP_SCRIPTS})
else()
    message(FATAL_ERROR "Babylon.js package is required. Please run 'npm install' first.")
endif()

set_property(DIRECTORY ${CMAKE_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT BabylonNativePlayground)

# Platform-specific settings
if(MSVC)
    target_compile_definitions(BabylonNativePlayground PRIVATE TARGET_PLATFORM_WINDOWS)
elseif(APPLE)
    target_compile_definitions(BabylonNativePlayground PRIVATE TARGET_PLATFORM_OSX)
    find_library(JSCORE_LIBRARY JavaScriptCore)
    target_link_libraries(BabylonNativePlayground PRIVATE ${JSCORE_LIBRARY})
    target_link_libraries(BabylonNativePlayground PRIVATE
        "-framework Cocoa"
        "-framework Foundation"
        "-framework MetalKit"
        "-framework QuartzCore"
        "-framework CoreFoundation"
        "-framework CoreGraphics")
else()
    target_compile_definitions(BabylonNativePlayground PRIVATE TARGET_PLATFORM_LINUX)
endif()
