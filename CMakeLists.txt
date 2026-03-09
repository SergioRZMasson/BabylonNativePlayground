cmake_minimum_required(VERSION 3.21)
project(BabylonNativePlayground VERSION 0.1.0)

# ---- Qt 6 (provided by vcpkg) ----
set(CMAKE_AUTOMOC ON)
find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_subdirectory(Dependencies)

add_executable(BabylonNativePlayground main.cpp)

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
    PRIVATE Qt6::Widgets
    ${ADDITIONAL_LIBRARIES}
    ${BABYLON_NATIVE_PLAYGROUND_EXTENSION_LIBRARIES})

target_compile_features(BabylonNativePlayground PRIVATE cxx_std_17)

# Copy Babylon.js scripts to build output
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/node_modules/babylonjs/babylon.max.js")
    set(BABYLON_SCRIPTS
        "node_modules/babylonjs/babylon.max.js"
        "node_modules/babylonjs-loaders/babylonjs.loaders.js"
        "node_modules/babylonjs-materials/babylonjs.materials.js")

    set(APP_SCRIPTS
        "Scripts/game.js")

    target_sources(BabylonNativePlayground PRIVATE ${BABYLON_SCRIPTS} ${APP_SCRIPTS})

    foreach(SCRIPT ${BABYLON_SCRIPTS} ${APP_SCRIPTS})
        get_filename_component(SCRIPT_NAME "${SCRIPT}" NAME)
        add_custom_command(
            OUTPUT "${CMAKE_CFG_INTDIR}/Scripts/${SCRIPT_NAME}"
            COMMAND "${CMAKE_COMMAND}" -E copy "${CMAKE_CURRENT_SOURCE_DIR}/${SCRIPT}" "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/Scripts/${SCRIPT_NAME}"
            COMMENT "Copying ${SCRIPT_NAME}"
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
        "-framework MetalKit"
        "-framework QuartzCore"
        "-framework CoreFoundation"
        "-framework CoreGraphics")
else()
    target_compile_definitions(BabylonNativePlayground PRIVATE TARGET_PLATFORM_LINUX)
endif()

# Deploy Qt runtime DLLs and plugins next to the executable on every build.
# Without this, Qt will assert / crash because it cannot find the platform
# plugin (qwindows.dll) or the style plugin needed for the theme.
# Note: vcpkg's applocal.ps1 copies regular DLLs automatically, but NOT the
# Qt plugin sub-directories (platforms/, styles/).  We copy them ourselves.
if(WIN32)
    # Resolve the vcpkg Qt6 plugin directory.
    # Qt6_DIR points to  <installed>/<triplet>/share/Qt6
    # plugins live under <installed>/<triplet>/Qt6/plugins  (release)
    #               and  <installed>/<triplet>/debug/Qt6/plugins  (debug)
    cmake_path(SET _qt6_plugins_release NORMALIZE "${Qt6_DIR}/../../Qt6/plugins")
    cmake_path(SET _qt6_plugins_debug   NORMALIZE "${Qt6_DIR}/../../debug/Qt6/plugins")

    # Pick the right plugin directory for the current configuration
    set(_qt6_plugins "$<IF:$<CONFIG:Debug>,${_qt6_plugins_debug},${_qt6_plugins_release}>")

    add_custom_command(TARGET BabylonNativePlayground POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:BabylonNativePlayground>/platforms"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:BabylonNativePlayground>/styles"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${_qt6_plugins}/platforms" "$<TARGET_FILE_DIR:BabylonNativePlayground>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${_qt6_plugins}/styles"    "$<TARGET_FILE_DIR:BabylonNativePlayground>/styles"
        COMMENT "Copying Qt platform & style plugins..."
    )

    # Install-time deployment script for packaging
    qt_generate_deploy_app_script(
        TARGET BabylonNativePlayground
        OUTPUT_SCRIPT deploy_script
        NO_UNSUPPORTED_PLATFORM_ERROR)
    install(SCRIPT ${deploy_script})
endif()
