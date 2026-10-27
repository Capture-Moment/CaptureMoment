# 🟦 Windows Build Guide
This guide is optimized to handle the challenges of building on Windows.

## 🛠️ Environment Setup with vcpkg

**VCPKG_ROOT :** Ensure this environment variable points to your Vcpkg installation.

### 1. Install Vcpkg
```powershell
git clone [https://github.com/Microsoft/vcpkg.git](https://github.com/Microsoft/vcpkg.git) C:\vcpkg
cd C:\vcpkg
.\bootstrap-vcpkg.bat
# Recommended:  Make VCPKG_ROOT permanent to system PATH
```

### 2. Enable Binary Caching (Crucial!)
To avoid recompiling LLVM every time you clean your project, enable local binary caching. Check if it's ON by default.

```PowerShell
# Create a permanent cache folder
mkdir C:\vcpkg_cache
# Set the variable (Make this permanent in Windows Environment Variables)
$env:VCPKG_BINARY_SOURCES = "clear;files,C:\vcpkg_cache"
```
---
## 🚀 Building the Project
Use the **Native Tools Command Prompt for VS**

We recommend using CMake Presets which automatically configure the correct generators and Vcpkg triplets. Don't forget to enable the UI with **-Ddesktop_ui=ON** if you want to compile also the ui
You can check [**The Main building**](../BUILDING_MAIN.md) to see all options

### Option A: Only the Core

**Optimized Mode (Release only) :** (Uses the custom x64-windows-release triplet for time/space saving)

```PowerShell
cmake --preset release-vcpkg-msvc
cmake --build --preset release-vcpkg-msvc
# or
cmake --preset release-vcpkg-mingw
cmake --build --preset release-vcpkg-mingw
```

For Full Development (Debug):
```PowerShell
cmake --preset debug-vcpkg-msvc
cmake --build --preset debug-vcpkg-msvc
```

### Option B: With Qt Desktop GUI

#### If you have Qt

```PowerShell
cmake --preset release-vcpkg-msvc -Ddesktop_ui="ON"
cmake --build --preset release-vcpkg-msvc
# or
cmake --preset release-vcpkg-mingw  -Ddesktop_ui="ON"
cmake --build --preset release-vcpkg-mingw
```

#### If you don't have Qt
```PowerShell
cmake --preset release-vcpkg-msvc-desktop
cmake --build --preset release-vcpkg-msvc-desktop
# or
cmake --preset release-vcpkg-mingw-desktop
cmake --build --preset release-vcpkg-mingw
```

#### Qt Creator
Open the root CMakeLists.txt with Qt Creator and choose the build vcpkg core msvc or mingw (with no desktop). And when you are in the pannel of the configuration, activate BUILD_DESKTOP_UI to ON

#### Visual Studio
Not configured at yet

## Custom Halide Path
```PowerShell
cmake --preset release-vcpkg-msvc -DHALIDE_DIR=/path/to/halide(cmake)
cmake --build --preset release-vcpkg-msvc
# or
cmake --preset release-vcpkg-mingw -DHALIDE_DIR=/path/to/halide(cmake)
cmake --build --preset release-vcpkg-mingw
```

---
## 🔍 Available Windows Presets
|     Preset Name     |       Generator       | Build Type |       VCPKG Triplet       |               Optimization / Notes              |
|:-------------------:|:---------------------:|:----------:|:-------------------------:|:-----------------------------------------------:|
| debug-vcpkg-msvc    | Auto-detected         | Debug      | x64-windows               | Standard Debug Build.                           |
| release-vcpkg-msvc  | Auto-detected         | Release    | x64-windows-release       | Optimized for Space/Time Savings (Recommended). |
| debug-vcpkg-mingw   | MinGW Makefiles       | Debug      | x64-mingw-dynamic         | For the MinGW toolchain.                        |
| release-vcpkg-mingw | MinGW Makefiles       | Release    | x64-mingw-dynamic-release | Optimized for MinGW (Time/Space Savings).       |
| debug-vs            | Visual Studio 18 2026 | Debug      | N/A                       | Without Vcpkg toolchain, uses default VS build. |
| release-vs          | Visual Studio 18 2026 | Release    | N/A                       | Without Vcpkg toolchain.                        |