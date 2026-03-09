# ---- Babylon Native ----
set(BABYLON_NATIVE_BUILD_APPS OFF CACHE BOOL "" FORCE)
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
