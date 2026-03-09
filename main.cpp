#include <functional>
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

#include <QApplication>
#include <QMainWindow>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QTimer>
#include <QMenuBar>
#include <QStatusBar>
#include <QToolBar>
#include <QDockWidget>
#include <QGroupBox>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QMessageBox>

// ---------------------------------------------------------------------------
// Babylon Native globals
// ---------------------------------------------------------------------------
static std::unique_ptr<Babylon::AppRuntime> runtime{};
static std::optional<Babylon::Graphics::Device> device{};
static std::optional<Babylon::Graphics::DeviceUpdate> update{};
static Babylon::Plugins::NativeInput* nativeInput{};
static std::unique_ptr<Babylon::Polyfills::Canvas> nativeCanvas{};

// ---------------------------------------------------------------------------
// RenderCanvas – a QWidget that serves as the native render target.
// Qt paints nothing on this widget; Babylon Native / bgfx renders directly
// into the underlying platform window obtained via winId().
// ---------------------------------------------------------------------------
class RenderCanvas : public QWidget
{
public:
    // Callbacks wired up by the host (no Q_OBJECT / signals needed)
    std::function<void(int, int)>               onResized;
    std::function<void(int, int32_t, int32_t)>  onMousePressed;
    std::function<void(int, int32_t, int32_t)>  onMouseReleased;
    std::function<void(int32_t, int32_t)>       onMouseMoved;
    std::function<void(int)>                    onWheelScrolled;

    explicit RenderCanvas(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        // Ensure a real native window is created for this widget so that
        // winId() returns a valid OS handle (HWND / XID / NSView).
        setAttribute(Qt::WA_NativeWindow);
        setAttribute(Qt::WA_PaintOnScreen);       // skip Qt painting
        setAttribute(Qt::WA_NoSystemBackground);  // avoid flicker
        setFocusPolicy(Qt::StrongFocus);
        setMouseTracking(true);
        setMinimumSize(320, 240);
    }

    // Prevent Qt from painting on the widget – bgfx owns the surface.
    QPaintEngine* paintEngine() const override { return nullptr; }

protected:
    void resizeEvent(QResizeEvent* event) override
    {
        QWidget::resizeEvent(event);
        if (onResized)
            onResized(event->size().width(), event->size().height());
    }

    void mousePressEvent(QMouseEvent* event) override
    {
        if (!onMousePressed) return;
        int id = MapButton(event->button());
        if (id >= 0)
            onMousePressed(id,
                static_cast<int32_t>(event->position().x()),
                static_cast<int32_t>(event->position().y()));
    }

    void mouseReleaseEvent(QMouseEvent* event) override
    {
        if (!onMouseReleased) return;
        int id = MapButton(event->button());
        if (id >= 0)
            onMouseReleased(id,
                static_cast<int32_t>(event->position().x()),
                static_cast<int32_t>(event->position().y()));
    }

    void mouseMoveEvent(QMouseEvent* event) override
    {
        if (onMouseMoved)
            onMouseMoved(
                static_cast<int32_t>(event->position().x()),
                static_cast<int32_t>(event->position().y()));
    }

    void wheelEvent(QWheelEvent* event) override
    {
        if (onWheelScrolled)
            onWheelScrolled(event->angleDelta().y());
    }

private:
    static int MapButton(Qt::MouseButton btn)
    {
        switch (btn)
        {
        case Qt::LeftButton:   return Babylon::Plugins::NativeInput::LEFT_MOUSE_BUTTON_ID;
        case Qt::RightButton:  return Babylon::Plugins::NativeInput::RIGHT_MOUSE_BUTTON_ID;
        case Qt::MiddleButton: return Babylon::Plugins::NativeInput::MIDDLE_MOUSE_BUTTON_ID;
        default:               return -1;
        }
    }
};

// ---------------------------------------------------------------------------
// Babylon initialization helpers
// ---------------------------------------------------------------------------
static void Uninitialize()
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

static void Initialize(RenderCanvas* canvas)
{
    Uninitialize();

    Babylon::Graphics::Configuration graphicsConfig{};
#if TARGET_PLATFORM_WINDOWS
    graphicsConfig.Window = reinterpret_cast<HWND>(canvas->winId());
#elif TARGET_PLATFORM_LINUX
    graphicsConfig.Window = static_cast<Babylon::Graphics::WindowT>(canvas->winId());
#elif TARGET_PLATFORM_OSX
    graphicsConfig.Window = reinterpret_cast<Babylon::Graphics::WindowT>(canvas->winId());
#endif
    graphicsConfig.Width  = static_cast<size_t>(canvas->width());
    graphicsConfig.Height = static_cast<size_t>(canvas->height());
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
    });

    Babylon::ScriptLoader loader{*runtime};
    loader.Eval("document = {}", "");
    loader.LoadScript("app:///Scripts/babylon.max.js");
    loader.LoadScript("app:///Scripts/babylonjs.loaders.js");
    loader.LoadScript("app:///Scripts/babylonjs.materials.js");
    loader.LoadScript("app:///Scripts/game.js");
}

// ---------------------------------------------------------------------------
// main – builds the Qt UI and starts the Babylon render loop
// ---------------------------------------------------------------------------
int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("Babylon Native Playground");
    app.setOrganizationName("BabylonNative");

    // ---- Main window -------------------------------------------------------
    QMainWindow mainWindow;
    mainWindow.setWindowTitle("Babylon Native Playground");
    mainWindow.resize(1280, 720);

    // ---- Menu bar ----------------------------------------------------------
    QMenu* fileMenu  = mainWindow.menuBar()->addMenu("&File");
    QAction* exitAct = fileMenu->addAction("E&xit");
    QObject::connect(exitAct, &QAction::triggered, &app, &QApplication::quit);

    QMenu* sceneMenu   = mainWindow.menuBar()->addMenu("&Scene");
    QAction* resetAct  = sceneMenu->addAction("&Reset Scene");

    QMenu* helpMenu    = mainWindow.menuBar()->addMenu("&Help");
    QAction* aboutAct  = helpMenu->addAction("&About");

    // ---- Toolbar -----------------------------------------------------------
    QToolBar* toolbar = mainWindow.addToolBar("Main");
    toolbar->setMovable(false);
    toolbar->addAction(resetAct);   // shared with menu

    // ---- Central widget: the render canvas ---------------------------------
    auto* centralWidget = new QWidget();
    auto* centralLayout = new QVBoxLayout(centralWidget);
    centralLayout->setContentsMargins(0, 0, 0, 0);

    auto* canvas = new RenderCanvas();
    centralLayout->addWidget(canvas);
    mainWindow.setCentralWidget(centralWidget);

    // ---- Left dock: property / info panel ----------------------------------
    auto* propsDock = new QDockWidget("Properties", &mainWindow);
    propsDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    propsDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);

    auto* propsWidget = new QWidget();
    auto* propsLayout = new QVBoxLayout(propsWidget);

    auto* sceneGroup  = new QGroupBox("Scene");
    auto* sceneLayout = new QVBoxLayout(sceneGroup);
    sceneLayout->addWidget(new QLabel("Babylon Native Playground"));
    sceneLayout->addWidget(new QLabel("Qt Edition"));
    propsLayout->addWidget(sceneGroup);

    auto* controlsGroup  = new QGroupBox("Controls");
    auto* controlsLayout = new QVBoxLayout(controlsGroup);
    controlsLayout->addWidget(new QLabel("Left Mouse: Rotate"));
    controlsLayout->addWidget(new QLabel("Right Mouse: Pan"));
    controlsLayout->addWidget(new QLabel("Scroll: Zoom"));
    propsLayout->addWidget(controlsGroup);

    propsLayout->addStretch();
    propsDock->setWidget(propsWidget);
    mainWindow.addDockWidget(Qt::LeftDockWidgetArea, propsDock);

    // ---- Status bar --------------------------------------------------------
    mainWindow.statusBar()->showMessage("Ready");

    // ---- Wire canvas events to Babylon Native ------------------------------
    canvas->onResized = [](int w, int h)
    {
        if (device)
            device->UpdateSize(static_cast<size_t>(w), static_cast<size_t>(h));
    };

    canvas->onMousePressed = [](int buttonId, int32_t x, int32_t y)
    {
        if (nativeInput)
            nativeInput->MouseDown(buttonId, x, y);
    };

    canvas->onMouseReleased = [](int buttonId, int32_t x, int32_t y)
    {
        if (nativeInput)
            nativeInput->MouseUp(buttonId, x, y);
    };

    canvas->onMouseMoved = [](int32_t x, int32_t y)
    {
        if (nativeInput)
            nativeInput->MouseMove(x, y);
    };

    canvas->onWheelScrolled = [](int angleDelta)
    {
        if (nativeInput)
        {
            // Qt gives ±120 per wheel notch; scale to match GLFW-style ±100
            int scaled = static_cast<int>(-angleDelta * 100.0 / 120.0);
            nativeInput->MouseWheel(
                Babylon::Plugins::NativeInput::MOUSEWHEEL_Y_ID, scaled);
        }
    };

    // ---- Menu / toolbar actions --------------------------------------------
    auto resetScene = [canvas, &mainWindow]()
    {
        Initialize(canvas);
        mainWindow.statusBar()->showMessage("Scene reset");
    };

    QObject::connect(resetAct, &QAction::triggered, resetScene);

    QObject::connect(aboutAct, &QAction::triggered, [&mainWindow]()
    {
        QMessageBox::about(&mainWindow, "About",
            "Babylon Native Playground\n"
            "Qt Edition\n\n"
            "A sample application embedding a Babylon.js 3-D "
            "scene inside a Qt window.");
    });

    // ---- Show the window then initialise Babylon ---------------------------
    mainWindow.show();

    // Use a single-shot timer so the native window handle is fully realised
    // before we hand it to bgfx / Babylon.
    QTimer::singleShot(0, [canvas]() { Initialize(canvas); });

    // ---- Render loop (≈60 fps) via QTimer ----------------------------------
    QTimer renderTimer;
    QObject::connect(&renderTimer, &QTimer::timeout, []()
    {
        if (device)
        {
            update->Finish();
            device->FinishRenderingCurrentFrame();
            device->StartRenderingCurrentFrame();
            update->Start();
        }
    });
    renderTimer.start(16);

    // ---- Run ---------------------------------------------------------------
    int result = app.exec();

    Uninitialize();
    return result;
}
