#include <filesystem>
#include <iostream>
#include <optional>

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

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#if TARGET_PLATFORM_LINUX
#define GLFW_EXPOSE_NATIVE_X11
#elif TARGET_PLATFORM_OSX
#define GLFW_EXPOSE_NATIVE_COCOA
#elif TARGET_PLATFORM_WINDOWS
#define GLFW_EXPOSE_NATIVE_WIN32
#endif
#include <GLFW/glfw3native.h>

static constexpr int INITIAL_WIDTH = 1280;
static constexpr int INITIAL_HEIGHT = 720;

std::unique_ptr<Babylon::AppRuntime> runtime{};
std::optional<Babylon::Graphics::Device> device{};
std::optional<Babylon::Graphics::DeviceUpdate> update{};
Babylon::Plugins::NativeInput* nativeInput{};
std::unique_ptr<Babylon::Polyfills::Canvas> nativeCanvas{};

static Babylon::Graphics::WindowT GetNativeWindowHandle(GLFWwindow* window)
{
#if TARGET_PLATFORM_LINUX
    return glfwGetX11Window(window);
#elif TARGET_PLATFORM_OSX
    return ((NSWindow*)glfwGetCocoaWindow(window)).contentView;
#elif TARGET_PLATFORM_WINDOWS
    return glfwGetWin32Window(window);
#endif
}

void Uninitialize()
{
    if (device)
    {
        update->Finish();
        device->FinishRenderingCurrentFrame();
    }

    nativeInput = {};
    runtime.reset();
    nativeCanvas.reset();
    update.reset();
    device.reset();
}

void Initialize(GLFWwindow* window)
{
    Uninitialize();

    int width, height;
    glfwGetWindowSize(window, &width, &height);

    Babylon::Graphics::Configuration graphicsConfig{};
    graphicsConfig.Window = GetNativeWindowHandle(window);
    graphicsConfig.Width = width;
    graphicsConfig.Height = height;
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
        nativeCanvas = std::make_unique<Babylon::Polyfills::Canvas>(Babylon::Polyfills::Canvas::Initialize(env));
        Babylon::Plugins::NativeEngine::Initialize(env);
        Babylon::Plugins::NativeOptimizations::Initialize(env);
        nativeInput = &Babylon::Plugins::NativeInput::CreateForJavaScript(env);
    });

    Babylon::ScriptLoader loader{*runtime};
    loader.Eval("document = {}", "");
    loader.LoadScript("app:///Scripts/babylon.max.js");
    loader.LoadScript("app:///Scripts/babylonjs.loaders.js");
    loader.LoadScript("app:///Scripts/babylonjs.materials.js");
    loader.LoadScript("app:///Scripts/game.js");
}

// --- GLFW callbacks ---

static void KeyCallback(GLFWwindow* window, int key, int /*scancode*/, int action, int /*mods*/)
{
    if (key == GLFW_KEY_R && action == GLFW_PRESS)
    {
        Initialize(window);
    }
}

static void MouseButtonCallback(GLFWwindow* window, int button, int action, int /*mods*/)
{
    double xpos, ypos;
    glfwGetCursorPos(window, &xpos, &ypos);
    int32_t x = static_cast<int32_t>(xpos);
    int32_t y = static_cast<int32_t>(ypos);

    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
        nativeInput->MouseDown(Babylon::Plugins::NativeInput::LEFT_MOUSE_BUTTON_ID, x, y);
    else if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE)
        nativeInput->MouseUp(Babylon::Plugins::NativeInput::LEFT_MOUSE_BUTTON_ID, x, y);
    else if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS)
        nativeInput->MouseDown(Babylon::Plugins::NativeInput::RIGHT_MOUSE_BUTTON_ID, x, y);
    else if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_RELEASE)
        nativeInput->MouseUp(Babylon::Plugins::NativeInput::RIGHT_MOUSE_BUTTON_ID, x, y);
    else if (button == GLFW_MOUSE_BUTTON_MIDDLE && action == GLFW_PRESS)
        nativeInput->MouseDown(Babylon::Plugins::NativeInput::MIDDLE_MOUSE_BUTTON_ID, x, y);
    else if (button == GLFW_MOUSE_BUTTON_MIDDLE && action == GLFW_RELEASE)
        nativeInput->MouseUp(Babylon::Plugins::NativeInput::MIDDLE_MOUSE_BUTTON_ID, x, y);
}

static void CursorPositionCallback(GLFWwindow* /*window*/, double xpos, double ypos)
{
    nativeInput->MouseMove(static_cast<int32_t>(xpos), static_cast<int32_t>(ypos));
}

static void ScrollCallback(GLFWwindow* /*window*/, double /*xoffset*/, double yoffset)
{
    nativeInput->MouseWheel(Babylon::Plugins::NativeInput::MOUSEWHEEL_Y_ID, static_cast<int>(-yoffset * 100.0));
}

static void WindowSizeCallback(GLFWwindow* /*window*/, int width, int height)
{
    device->UpdateSize(width, height);
}

int main()
{
    if (!glfwInit())
    {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return EXIT_FAILURE;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    GLFWwindow* window = glfwCreateWindow(INITIAL_WIDTH, INITIAL_HEIGHT, "Babylon Native Playground", nullptr, nullptr);
    if (!window)
    {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return EXIT_FAILURE;
    }

    glfwSetKeyCallback(window, KeyCallback);
    glfwSetWindowSizeCallback(window, WindowSizeCallback);
    glfwSetCursorPosCallback(window, CursorPositionCallback);
    glfwSetMouseButtonCallback(window, MouseButtonCallback);
    glfwSetScrollCallback(window, ScrollCallback);

    Initialize(window);

    while (!glfwWindowShouldClose(window))
    {
        if (device)
        {
            update->Finish();
            device->FinishRenderingCurrentFrame();
            device->StartRenderingCurrentFrame();
            update->Start();
        }
        glfwPollEvents();
    }

    Uninitialize();

    glfwDestroyWindow(window);
    glfwTerminate();
    return EXIT_SUCCESS;
}
