# Babylon Native Playground

A simple cross-platform C++ application using [Babylon Native](https://github.com/BabylonJS/BabylonNative) and [GLFW](https://github.com/glfw/glfw) to render a 3D scene. The app creates a GLFW window, initializes the Babylon Native runtime, and renders a basic scene defined in JavaScript (`Scripts/game.js`).

## Prerequisites

- **CMake** 3.21 or later
- **Node.js** and **npm**
- **C++17 compiler**

### Platform-specific requirements

**Windows:**
- Visual Studio 2019 or later with the "Desktop development with C++" workload

**macOS:**
- Xcode 12 or later

**Linux:**
```bash
sudo apt-get install libxi-dev libxcursor-dev libxinerama-dev libxrandr-dev \
    libgl1-mesa-dev libcurl4-openssl-dev clang-9 libc++-9-dev libc++abi-9-dev \
    lld-9 ninja-build libv8-dev
```

## Getting Started

### 1. Clone the repository

```bash
git clone --recursive https://github.com/<your-username>/BabylonNativeOfflinePlayground.git
cd BabylonNativeOfflinePlayground
```

If you already cloned without `--recursive`:
```bash
git submodule update --init --recursive
```

### 2. Install npm dependencies

```bash
npm install
```

### 3. Build

**Windows (Visual Studio):**
```bash
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022"
cmake --build . --config Release
```

**macOS (Xcode):**
```bash
mkdir build && cd build
cmake .. -G "Xcode"
cmake --build . --config Release
```

**Linux (Ninja):**
```bash
mkdir build && cd build
cmake -G Ninja -DNAPI_JAVASCRIPT_ENGINE=V8 ..
ninja
```

### 4. Run

**Windows:**
```bash
.\build\Release\BabylonNativePlayground.exe
```

**macOS:**
```bash
./build/Release/BabylonNativePlayground
```

**Linux:**
```bash
./build/BabylonNativePlayground
```

## Controls

- **Left mouse button** — Rotate camera
- **Right mouse button** — Pan camera
- **Scroll wheel** — Zoom in/out
- **R key** — Reload the scene

## Customizing the Scene

Edit `Scripts/game.js` to modify the rendered scene. The file uses the Babylon.js API to create meshes, lights, cameras, and materials. Changes take effect after pressing **R** or restarting the application.

## Project Structure

```
├── .github/workflows/    # CI pipeline (GitHub Actions)
├── Dependencies/
│   ├── BabylonNative/    # Git submodule
│   ├── glfw/             # Git submodule
│   └── CMakeLists.txt    # Dependency configuration
├── Scripts/
│   └── game.js           # Babylon.js scene script
├── CMakeLists.txt        # Root build configuration
├── main.cpp              # Application entry point
├── package.json          # npm dependencies (babylonjs)
└── README.md
```

## License

This project is provided as-is for educational purposes. Babylon Native is licensed under the [Apache 2.0 License](https://github.com/BabylonJS/BabylonNative/blob/master/LICENSE.md). GLFW is licensed under the [zlib/libpng License](https://www.glfw.org/license.html).
