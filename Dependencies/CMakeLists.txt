# ---- SDL3 ----
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(SDL)
set_property(TARGET SDL3-static PROPERTY FOLDER Dependencies/SDL)

# ---- Babylon Native ----
set(BABYLON_NATIVE_BUILD_APPS OFF CACHE BOOL "" FORCE)
set(BABYLON_NATIVE_PLUGIN_TESTUTILS OFF CACHE BOOL "" FORCE)
set(BABYLON_NATIVE_PLUGIN_SHADERTOOL OFF CACHE BOOL "" FORCE)
set(BABYLON_NATIVE_PLUGIN_NATIVEXR OFF CACHE BOOL "" FORCE)
set(BABYLON_NATIVE_PLUGIN_NATIVECAMERA OFF CACHE BOOL "" FORCE)
set(BABYLON_NATIVE_PLUGIN_NATIVETRACING OFF CACHE BOOL "" FORCE)

add_subdirectory(BabylonNative)

# Organize targets in IDE folders
set_property(TARGET AppRuntime PROPERTY FOLDER Dependencies/BabylonNative/Core/)
if(TARGET Graphics)
    set_property(TARGET Graphics PROPERTY FOLDER Dependencies/BabylonNative/Core/)
endif()
set_property(TARGET JsRuntime PROPERTY FOLDER Dependencies/BabylonNative/Core/)
set_property(TARGET ScriptLoader PROPERTY FOLDER Dependencies/BabylonNative/Core/)

if(TARGET NativeEngine)
    set_property(TARGET NativeEngine PROPERTY FOLDER Dependencies/BabylonNative/Plugins/)
endif()

if(TARGET NativeInput)
    set_property(TARGET NativeInput PROPERTY FOLDER Dependencies/BabylonNative/Plugins/)
endif()

if(TARGET NativeOptimizations)
    set_property(TARGET NativeOptimizations PROPERTY FOLDER Dependencies/BabylonNative/Plugins/)
endif()

if(TARGET Console)
    set_property(TARGET Console PROPERTY FOLDER Dependencies/BabylonNative/Polyfills/)
endif()

if(TARGET Window)
    set_property(TARGET Window PROPERTY FOLDER Dependencies/BabylonNative/Polyfills/)
endif()

if(TARGET XMLHttpRequest)
    set_property(TARGET XMLHttpRequest PROPERTY FOLDER Dependencies/BabylonNative/Polyfills/)
endif()

if(TARGET Canvas)
    set_property(TARGET Canvas PROPERTY FOLDER Dependencies/BabylonNative/Polyfills/)
endif()

# ---- Apple Framework Workaround ----
# BabylonNative's JsRuntimeHost creates a CMake target named "Foundation" which
# collides with macOS's Foundation.framework. SDL3 links the framework via
# $<LINK_LIBRARY:FRAMEWORK,Foundation>, but CMake resolves "Foundation" to the
# JsRuntimeHost target, producing the broken flag "-framework libFoundation.a".
# Fix by replacing the generator expression with a plain -framework flag in both
# LINK_LIBRARIES and INTERFACE_LINK_LIBRARIES (CMake 3.26+ propagates PRIVATE
# static-library deps into INTERFACE_LINK_LIBRARIES via $<LINK_ONLY:...>).
if(APPLE)
    foreach(_sdl_prop LINK_LIBRARIES INTERFACE_LINK_LIBRARIES)
        get_target_property(_sdl3_libs SDL3-static ${_sdl_prop})
        if(_sdl3_libs)
            string(REPLACE "$<LINK_LIBRARY:FRAMEWORK,Foundation>" "-framework Foundation" _sdl3_libs "${_sdl3_libs}")
            set_target_properties(SDL3-static PROPERTIES ${_sdl_prop} "${_sdl3_libs}")
        endif()
    endforeach()
endif()

# ---- ImGui ----
add_subdirectory(imgui)
set_property(TARGET imgui PROPERTY FOLDER Dependencies/)
