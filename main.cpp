#include <functional>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <mutex>
#include <vector>
#include <algorithm>

#include <Babylon/AppRuntime.h>
#include <Babylon/Graphics/Device.h>
#include <Babylon/ScriptLoader.h>
#include <Babylon/Plugins/NativeEngine.h>
#include <Babylon/Plugins/NativeOptimizations.h>
#include <Babylon/Plugins/NativeInput.h>
#include <Babylon/Polyfills/Console.h>
#include <Babylon/Polyfills/Window.h>
#include <Babylon/Polyfills/XMLHttpRequest.h>
#include <Babylon/Polyfills/Canvas.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_dialog.h>
#ifdef TARGET_PLATFORM_OSX
#include <SDL3/SDL_metal.h>
#endif

#include "imgui.h"
#include "backends/imgui_impl_sdl3.h"
#include "backends/imgui_impl_babylon.h"
#include "SceneInspector.h"

// ---------------------------------------------------------------------------
// Babylon Native globals
// ---------------------------------------------------------------------------
static std::unique_ptr<Babylon::AppRuntime> runtime{};
static std::optional<Babylon::Graphics::Device> device{};
static std::optional<Babylon::Graphics::DeviceUpdate> update{};
static Babylon::Plugins::NativeInput* nativeInput{};
static std::unique_ptr<Babylon::Polyfills::Canvas> nativeCanvas{};

static bool s_showImgui = true;

#ifdef TARGET_PLATFORM_OSX
static SDL_MetalView s_metalView{};
#endif

static std::mutex s_sceneDataMutex;
static std::vector<uint8_t> s_sceneDataBuffer;
static bool s_sceneDataDirty = false;
static SceneInspector::SceneData s_parsedSceneData;

// Command dispatch state
static std::mutex s_cmdMutex;
static std::vector<uint8_t> s_pendingCmdBuffer;

// Code editor sync state
static std::mutex s_codeSyncMutex;
static std::string s_pendingCode;
static bool s_codeSyncDirty = false;

// ---------------------------------------------------------------------------
// Native callback: receive playground code from JS to display in editor
// ---------------------------------------------------------------------------
static void NativeSetPlaygroundCode(const Napi::CallbackInfo& info)
{
    if (info.Length() < 1 || !info[0].IsString()) return;
    std::string code = info[0].As<Napi::String>().Utf8Value();

    std::lock_guard<std::mutex> lock(s_codeSyncMutex);
    s_pendingCode = std::move(code);
    s_codeSyncDirty = true;
}

// ---------------------------------------------------------------------------
// Native callback: receive serialized scene data from JS
// ---------------------------------------------------------------------------
static void NativeSetSceneData(const Napi::CallbackInfo& info)
{
    if (info.Length() < 1 || !info[0].IsArrayBuffer()) return;
    auto buffer = info[0].As<Napi::ArrayBuffer>();
    auto* data = static_cast<const uint8_t*>(buffer.Data());
    auto size = buffer.ByteLength();

    std::lock_guard<std::mutex> lock(s_sceneDataMutex);
    s_sceneDataBuffer.assign(data, data + size);
    s_sceneDataDirty = true;
}

// ---------------------------------------------------------------------------
// Get native window handle from SDL
// ---------------------------------------------------------------------------
static void* GetNativeWindowHandle(SDL_Window* window)
{
#if TARGET_PLATFORM_WINDOWS
    return (void*)SDL_GetPointerProperty(
        SDL_GetWindowProperties(window),
        SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
#elif TARGET_PLATFORM_LINUX
    return (void*)(uintptr_t)SDL_GetNumberProperty(
        SDL_GetWindowProperties(window),
        SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0);
#elif TARGET_PLATFORM_OSX
    if (!s_metalView)
    {
        s_metalView = SDL_Metal_CreateView(window);
    }
    return SDL_Metal_GetLayer(s_metalView);
#else
    return nullptr;
#endif
}

// ---------------------------------------------------------------------------
// Send playground hash to JS
// ---------------------------------------------------------------------------
static void LoadPlayground(const std::string& hash)
{
    if (!runtime) return;
    runtime->Dispatch([hash](Napi::Env env)
    {
        auto fn = env.Global().Get("LoadPlayground");
        if (fn.IsFunction())
        {
            fn.As<Napi::Function>().Call({Napi::String::New(env, hash)});
        }
    });
}

// ---------------------------------------------------------------------------
// Send raw playground code to JS
// ---------------------------------------------------------------------------
static void RunPlaygroundCode(const std::string& code)
{
    if (!runtime) return;
    runtime->Dispatch([code](Napi::Env env)
    {
        auto fn = env.Global().Get("RunPlaygroundCode");
        if (fn.IsFunction())
        {
            fn.As<Napi::Function>().Call({Napi::String::New(env, code)});
        }
    });
}

// ---------------------------------------------------------------------------
// Dispatch inspector commands to JS
// ---------------------------------------------------------------------------
static void DispatchInspectorCommands()
{
    if (!runtime) return;

    std::vector<uint8_t> cmdBuf;
    {
        std::lock_guard<std::mutex> lock(s_cmdMutex);
        if (s_pendingCmdBuffer.empty()) return;
        cmdBuf.swap(s_pendingCmdBuffer);
    }

    runtime->Dispatch([buf = std::move(cmdBuf)](Napi::Env env)
    {
        auto fn = env.Global().Get("ApplyInspectorCommands");
        if (fn.IsFunction())
        {
            auto ab = Napi::ArrayBuffer::New(env, buf.size());
            std::memcpy(ab.Data(), buf.data(), buf.size());
            fn.As<Napi::Function>().Call({ab});
        }
    });
}

// ---------------------------------------------------------------------------
// Load a dropped file (GLB or OBJ) and send contents to JS
// ---------------------------------------------------------------------------
static void LoadDroppedFile(const std::string& filePath)
{
    if (!runtime) return;

    // Get file extension (lowercase)
    std::string ext;
    auto dotPos = filePath.rfind('.');
    if (dotPos != std::string::npos)
    {
        ext = filePath.substr(dotPos);
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    }

    // Determine loader function name
    std::string loaderFunc;
    if (ext == ".glb" || ext == ".gltf")
        loaderFunc = "load_glb";
    else if (ext == ".obj")
        loaderFunc = "load_obj";
    else if (ext == ".env")
        loaderFunc = "load_env";
    else
    {
        std::cout << "Unsupported file type: " << ext << std::endl;
        return;
    }

    // Read the file into memory
    std::ifstream file(filePath, std::ios::binary | std::ios::ate);
    if (!file.is_open())
    {
        std::cerr << "Failed to open file: " << filePath << std::endl;
        return;
    }

    auto fileSize = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> fileData(static_cast<size_t>(fileSize));
    if (!file.read(reinterpret_cast<char*>(fileData.data()), fileSize))
    {
        std::cerr << "Failed to read file: " << filePath << std::endl;
        return;
    }
    file.close();

    // Extract just the filename for display
    std::string fileName = filePath;
    auto slashPos = fileName.find_last_of("\\/");
    if (slashPos != std::string::npos)
        fileName = fileName.substr(slashPos + 1);

    std::cout << "Loading dropped file: " << fileName
              << " (" << fileData.size() << " bytes)" << std::endl;

    runtime->Dispatch([data = std::move(fileData), loaderFunc, fileName](Napi::Env env)
    {
        auto fn = env.Global().Get(loaderFunc);
        if (!fn.IsFunction())
        {
            std::cerr << "JS function " << loaderFunc << " not found" << std::endl;
            return;
        }

        auto ab = Napi::ArrayBuffer::New(env, data.size());
        std::memcpy(ab.Data(), data.data(), data.size());

        fn.As<Napi::Function>().Call({
            ab,
            Napi::String::New(env, fileName)
        });
    });
}

// ---------------------------------------------------------------------------
// SDL3 file dialog callback — called when user picks a file
// ---------------------------------------------------------------------------
static void SDLCALL FileDialogCallback(void* userdata, const char* const* filelist, int filter)
{
    (void)userdata;
    (void)filter;
    if (!filelist || !filelist[0]) return;
    LoadDroppedFile(std::string(filelist[0]));
}

// ---------------------------------------------------------------------------
// Babylon initialization helpers
// ---------------------------------------------------------------------------
static void Uninitialize()
{
    {
        std::lock_guard<std::mutex> lock(s_sceneDataMutex);
        s_sceneDataBuffer.clear();
        s_sceneDataDirty = false;
    }
    {
        std::lock_guard<std::mutex> lock(s_cmdMutex);
        s_pendingCmdBuffer.clear();
    }
    s_parsedSceneData = {};

    if (device)
    {
        update->Finish();
        device->FinishRenderingCurrentFrame();
        ImGui_ImplBabylon_Shutdown();
    }

    nativeInput = {};
    runtime.reset();
    nativeCanvas.reset();
    update.reset();
    device.reset();
}

static void Initialize(SDL_Window* window)
{
    Uninitialize();

    int width, height;
    SDL_GetWindowSize(window, &width, &height);

    Babylon::Graphics::Configuration graphicsConfig{};
    graphicsConfig.Window = reinterpret_cast<Babylon::Graphics::WindowT>(GetNativeWindowHandle(window));
    graphicsConfig.Width  = static_cast<size_t>(width);
    graphicsConfig.Height = static_cast<size_t>(height);
    graphicsConfig.MSAASamples = 4;

    device.emplace(graphicsConfig);
    update.emplace(device->GetUpdate("update"));
    device->StartRenderingCurrentFrame();
    update->Start();

    runtime = std::make_unique<Babylon::AppRuntime>();

    runtime->Dispatch([](Napi::Env env)
    {
        device->AddToJavaScript(env);

        Babylon::Polyfills::Console::Initialize(env, [](const char* message, auto) {
            std::cout << message << std::endl;
        });

        Babylon::Polyfills::Window::Initialize(env);
        Babylon::Polyfills::XMLHttpRequest::Initialize(env);
        nativeCanvas = std::make_unique<Babylon::Polyfills::Canvas>(
            Babylon::Polyfills::Canvas::Initialize(env));
        Babylon::Plugins::NativeEngine::Initialize(env);
        Babylon::Plugins::NativeOptimizations::Initialize(env);
        nativeInput = &Babylon::Plugins::NativeInput::CreateForJavaScript(env);

        env.Global().Set("_nativeSetSceneData",
            Napi::Function::New(env, NativeSetSceneData));
        env.Global().Set("_nativeSetPlaygroundCode",
            Napi::Function::New(env, NativeSetPlaygroundCode));

        auto context = &Babylon::Graphics::DeviceContext::GetFromJavaScript(env);
        ImGui_ImplBabylon_SetContext(context);
    });

    Babylon::ScriptLoader loader{*runtime};
    loader.Eval("document = {}", "");
    loader.LoadScript("app:///Scripts/babylon.max.js");
    loader.LoadScript("app:///Scripts/babylonjs.loaders.js");
    loader.LoadScript("app:///Scripts/babylonjs.materials.js");
    loader.LoadScript("app:///Scripts/playground.js");
    loader.LoadScript("app:///Scripts/Loaders/loader_glb.js");
    loader.LoadScript("app:///Scripts/Loaders/loader_obj.js");
    loader.LoadScript("app:///Scripts/Loaders/loader_env.js");

    ImGui_ImplBabylon_Init(width, height);
}

// ---------------------------------------------------------------------------
// Map SDL mouse button to Babylon Native input ID
// ---------------------------------------------------------------------------
static int MapMouseButton(Uint8 button)
{
    switch (button)
    {
    case SDL_BUTTON_LEFT:   return Babylon::Plugins::NativeInput::LEFT_MOUSE_BUTTON_ID;
    case SDL_BUTTON_RIGHT:  return Babylon::Plugins::NativeInput::RIGHT_MOUSE_BUTTON_ID;
    case SDL_BUTTON_MIDDLE: return Babylon::Plugins::NativeInput::MIDDLE_MOUSE_BUTTON_ID;
    default:                return -1;
    }
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main(int argc, char* argv[])
{
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS))
    {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << std::endl;
        return 1;
    }

    SDL_WindowFlags windowFlags = SDL_WINDOW_RESIZABLE;
#ifdef TARGET_PLATFORM_OSX
    windowFlags |= SDL_WINDOW_METAL;
#endif

    SDL_Window* window = SDL_CreateWindow(
        "Babylon Native Playground",
        1280, 720,
        windowFlags);

    if (!window)
    {
        std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return 1;
    }

    // Setup ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();

    ImGui_ImplSDL3_InitForOther(window);

    // Initialize Babylon
    Initialize(window);

    // Playground hash input buffer
    static char hashBuf[256] = "";

    // Code editor buffer with a default createScene template
    static const char* defaultCode =
        "var createScene = function (engine) {\n"
        "    var scene = new BABYLON.Scene(engine);\n"
        "    scene.createDefaultCamera(true, true, true);\n"
        "    var camera = scene.activeCamera;\n"
        "    camera.setTarget(BABYLON.Vector3.Zero());\n"
        "    camera.position = new BABYLON.Vector3(0, 5, -10);\n"
        "\n"
        "    var light = new BABYLON.HemisphericLight(\n"
        "        \"light\", new BABYLON.Vector3(0, 1, 0), scene);\n"
        "    light.intensity = 0.7;\n"
        "\n"
        "    var sphere = BABYLON.MeshBuilder.CreateSphere(\n"
        "        \"sphere\", { diameter: 2, segments: 32 }, scene);\n"
        "    sphere.position.y = 1;\n"
        "\n"
        "    BABYLON.MeshBuilder.CreateGround(\n"
        "        \"ground\", { width: 6, height: 6 }, scene);\n"
        "\n"
        "    return scene;\n"
        "};\n";

    static constexpr size_t CODE_BUF_SIZE = 64 * 1024;
    static char codeBuf[CODE_BUF_SIZE];
    static bool codeInitialized = false;
    if (!codeInitialized)
    {
        snprintf(codeBuf, CODE_BUF_SIZE, "%s", defaultCode);
        codeInitialized = true;
    }

    bool running = true;
    while (running)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            ImGui_ImplSDL3_ProcessEvent(&event);

            switch (event.type)
            {
            case SDL_EVENT_QUIT:
                running = false;
                break;

            case SDL_EVENT_WINDOW_RESIZED:
                if (device)
                    device->UpdateSize(event.window.data1, event.window.data2);
                break;

            case SDL_EVENT_KEY_DOWN:
                if (event.key.key == SDLK_F1)
                    s_showImgui = !s_showImgui;
                if (event.key.key == SDLK_R && (event.key.mod & SDL_KMOD_CTRL))
                    Initialize(window);
                break;

            case SDL_EVENT_MOUSE_BUTTON_DOWN:
                if (!io.WantCaptureMouse && nativeInput)
                {
                    int id = MapMouseButton(event.button.button);
                    if (id >= 0)
                        nativeInput->MouseDown(id,
                            static_cast<int32_t>(event.button.x),
                            static_cast<int32_t>(event.button.y));
                }
                break;

            case SDL_EVENT_MOUSE_BUTTON_UP:
                if (!io.WantCaptureMouse && nativeInput)
                {
                    int id = MapMouseButton(event.button.button);
                    if (id >= 0)
                        nativeInput->MouseUp(id,
                            static_cast<int32_t>(event.button.x),
                            static_cast<int32_t>(event.button.y));
                }
                break;

            case SDL_EVENT_MOUSE_MOTION:
                if (!io.WantCaptureMouse && nativeInput)
                    nativeInput->MouseMove(
                        static_cast<int32_t>(event.motion.x),
                        static_cast<int32_t>(event.motion.y));
                break;

            case SDL_EVENT_MOUSE_WHEEL:
                if (!io.WantCaptureMouse && nativeInput)
                    nativeInput->MouseWheel(
                        Babylon::Plugins::NativeInput::MOUSEWHEEL_Y_ID,
                        static_cast<int>(-event.wheel.y * 100.0f));
                break;

            case SDL_EVENT_DROP_FILE:
                if (event.drop.data)
                {
                    std::string droppedFile(event.drop.data);
                    LoadDroppedFile(droppedFile);
                }
                break;
            }
        }

        // Render Babylon frame
        if (device)
        {
            update->Finish();
            device->FinishRenderingCurrentFrame();
            device->StartRenderingCurrentFrame();
            update->Start();
        }

        // ImGui frame
        if (s_showImgui)
        {
            ImGui_ImplBabylon_NewFrame();
            ImGui_ImplSDL3_NewFrame();
            ImGui::NewFrame();

            // Begin command buffer for this frame
            SceneInspector::BeginCommands();

            // Playground panel — fixed on the left side
            float leftPanelWidth = io.DisplaySize.x * 0.2f;
            if (leftPanelWidth < 280) leftPanelWidth = 280;
            ImGui::SetNextWindowPos(ImVec2(0, 0));
            ImGui::SetNextWindowSize(ImVec2(leftPanelWidth, io.DisplaySize.y));
            ImGui::Begin("Babylon Native Playground", nullptr,
                ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoCollapse);

            // =============================================================
            // Load Assets section
            // =============================================================
            if (ImGui::CollapsingHeader("Load Assets", ImGuiTreeNodeFlags_DefaultOpen))
            {
                if (ImGui::Button("Open GLB File..."))
                {
                    static const SDL_DialogFileFilter glbFilter[] = {
                        { "glTF Binary", "glb;gltf" }
                    };
                    SDL_ShowOpenFileDialog(FileDialogCallback, nullptr, window, glbFilter, 1, nullptr, false);
                }
                ImGui::SameLine();
                if (ImGui::Button("Open ENV File..."))
                {
                    static const SDL_DialogFileFilter envFilter[] = {
                        { "Environment Map", "env" }
                    };
                    SDL_ShowOpenFileDialog(FileDialogCallback, nullptr, window, envFilter, 1, nullptr, false);
                }
                ImGui::TextDisabled("or drag & drop .glb / .obj / .env files");
            }

            // =============================================================
            // Load Playground section
            // =============================================================
            if (ImGui::CollapsingHeader("Load Playground", ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::Text("Playground Hash:");
                ImGui::InputText("##hash", hashBuf, sizeof(hashBuf));
                ImGui::SameLine();
                if (ImGui::Button("Load"))
                {
                    std::string hash(hashBuf);
                    if (!hash.empty())
                        LoadPlayground(hash);
                }
            }

            ImGui::Separator();

            // Sync code editor if JS sent new playground code
            {
                std::lock_guard<std::mutex> lock(s_codeSyncMutex);
                if (s_codeSyncDirty)
                {
                    snprintf(codeBuf, CODE_BUF_SIZE, "%s", s_pendingCode.c_str());
                    s_codeSyncDirty = false;
                }
            }

            // =============================================================
            // Code Editor section
            // =============================================================
            ImGui::Separator();
            ImGui::Text("Code Editor:");
            ImVec2 avail = ImGui::GetContentRegionAvail();
            float editorHeight = avail.y - 80.0f;
            if (editorHeight < 100.0f) editorHeight = 100.0f;
            ImGui::InputTextMultiline("##code", codeBuf, CODE_BUF_SIZE,
                ImVec2(-1.0f, editorHeight),
                ImGuiInputTextFlags_AllowTabInput);

            if (ImGui::Button("Run Code"))
            {
                std::string code(codeBuf);
                if (!code.empty())
                    RunPlaygroundCode(code);
            }
            ImGui::SameLine();
            ImGui::Text("%.1f FPS", io.Framerate);
            ImGui::SameLine();
            ImGui::TextDisabled("(F1: toggle | Ctrl+R: reset)");

            ImGui::End();

            // Parse scene data if new data arrived from JS
            {
                std::lock_guard<std::mutex> lock(s_sceneDataMutex);
                if (s_sceneDataDirty)
                {
                    s_parsedSceneData = SceneInspector::Parse(
                        s_sceneDataBuffer.data(), s_sceneDataBuffer.size());
                    s_sceneDataDirty = false;
                }
            }
            SceneInspector::RenderInspector(s_parsedSceneData);

            // Finalize and dispatch commands
            SceneInspector::EndCommands();
            if (SceneInspector::HasPendingCommands())
            {
                std::lock_guard<std::mutex> lock(s_cmdMutex);
                auto* cmdData = SceneInspector::s_cmdWriter.data();
                auto  cmdSize = SceneInspector::s_cmdWriter.size();
                s_pendingCmdBuffer.assign(cmdData, cmdData + cmdSize);
            }

            ImGui::Render();
            ImGui_ImplBabylon_RenderDrawData(ImGui::GetDrawData());

            // Dispatch commands to JS after render
            if (SceneInspector::HasPendingCommands())
                DispatchInspectorCommands();
        }
    }

    Uninitialize();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

#ifdef TARGET_PLATFORM_OSX
    if (s_metalView)
    {
        SDL_Metal_DestroyView(s_metalView);
        s_metalView = nullptr;
    }
#endif

    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
