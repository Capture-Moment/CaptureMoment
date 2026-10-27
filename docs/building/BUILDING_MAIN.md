### `docs/BUILDING_MAIN.md`
*The main entry point for build instructions.*


# 🚀 Build Guide - CaptureMoment

Welcome! This guide will help you build CaptureMoment from source.

Due to complex dependencies (specifically Halide and LLVM), instructions are separated by operating system to ensure the best performance and avoid build issues.

## 🧰 Global Prerequisites

Before selecting your platform, ensure you have these tools:

1.  **CMake 4.2+**
2.  **C++23 Compiler**:
    * **Windows**: MSVC 2026 or MinGW (GCC 16+).
    * **Linux**: GCC 16+
    * **macOS**: Apple Clang 16+ (Xcode 15+).

---
## ⚙️ Global Build Configuration

### CMake Build
|  Component | Default Status |     CMake Variable (Alias)    |                  Description                  |
|:----------:|:--------------:|:-----------------------------:|:---------------------------------------------:|
| Desktop UI | OFF            | BUILD_DESKTOP_UI (desktop_ui) | Builds the Qt Quick/QML desktop application.  |
| Mobile UI  | OFF            | BUILD_MOBILE_UI (mobile_ui)   | Builds the mobile application (future phase). |
| Tests      | OFF            | BUILD_TESTS (tests)           | Builds unit and integration tests.            |
| Benchmarks | OFF            | BUILD_BENCHMARKS (benchmarks) | Builds performance benchmarks.                |

### Libraries

| Library     | Version      | Link                                                     |
|-------------|:------------------:|----------------------------------------------------------|
| OpenImageIO | 3.1.14.0             | https://github.com/AcademySoftwareFoundation/OpenImageIO |
| Halide      | 21.0           | https://github.com/halide/Halide                         |
| spdlog      | 1.16.0      | https://github.com/gabime/spdlog                         |
| Exiv2         | 0.28.7| https://github.com/Exiv2/exiv2  
| Magic Enum         |0.9.7| https://github.com/Neargye/magic_enum/
|  Qt6           | 6.10.x| https://doc.qt.io/qt-6/

---
## 📦 How to build

### 1. Build Halide
[➡️ Read the guide for Halide](INSTALLATION_HALIDE.md).

### 2. Setup Qt (Optional - Required for UI only)
If you want to build the Desktop UI. Please follow the dedicated guide to install `Qt` on your machine:  [➡️ Read the Building Qt Guide](INSTALLATION_QT.md).

### 3. Setup Dependencies & Compile
Click the link for your OS for detailed instructions:

* [🟦 **Windows**](BUILDING_WINDOWS.md).
* [🐧 **Linux**](/BUILDING_LINUX.md) (Ubuntu, Fedora, Arch).
* [🍎 **macOS**](/BUILDING_MACOS.md) (Homebrew and Xcode).

### 4. Run the tests
See the follow guide

* [🛠️ **Tests**](../TESTS.md).
* 
## Understood the core 

* [**The Core**](../architecture/CORE_DESIGN.md) (You can see the design patterns used for the core).
