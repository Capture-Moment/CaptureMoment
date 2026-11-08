#!/bin/bash

# ==============================================================================
# CaptureMoment - macOS Local Setup Script (Homebrew)
#
# Automates the setup of the build environment, installs Halide in a project
# virtualenv and compiles the required core dependencies (Exiv2, OpenImageIO)
# from source.
#
# Requires Homebrew: https://brew.sh
# Not covered by this script (user responsibility, see the Build Guide):
#   - CMake 4.2+   : brew install cmake
#   - Ninja        : brew install ninja
# ==============================================================================

set -e

EXIV2_VERSION="v0.28.7"
OIIO_VERSION="v3.1.14.0"
HALIDE_VERSION="22.1.0"
INSTALL_PREFIX="/opt"

# Halide ships as a pip wheel, installed in a virtualenv at the repo root so it
# stays isolated from the system/asdf Python. CMake picks it up automatically.
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VENV_DIR="${REPO_ROOT}/.venv"
# The Halide wheels are cp312-abi3: Python 3.12+ is required.
PYTHON_MIN_MINOR=12

echo "CaptureMoment - macOS setup (Homebrew)"
echo "Exiv2: ${INSTALL_PREFIX}/exiv2"
echo "OpenImageIO: ${INSTALL_PREFIX}/oiio"
echo "Halide ${HALIDE_VERSION}: ${VENV_DIR}"

# Shell config file: zsh is the default on macOS since Catalina
if [[ "${SHELL}" == *zsh* ]]; then
    SHELL_RC="${HOME}/.zshrc"
else
    SHELL_RC="${HOME}/.bashrc"
fi
echo "Environment will be persisted to: ${SHELL_RC}"

# ==============================================================================
# Prerequisites check (not installed by this script - user responsibility)
# ==============================================================================
MISSING=""
command -v cmake >/dev/null 2>&1 || MISSING="${MISSING} cmake(4.2+)"
command -v ninja >/dev/null 2>&1 || MISSING="${MISSING} ninja"
command -v python3 >/dev/null 2>&1 || MISSING="${MISSING} python"
command -v git >/dev/null 2>&1 || MISSING="${MISSING} git"
command -v brew >/dev/null 2>&1 || MISSING="${MISSING} homebrew"

# Default Apple Clang does not fully cover C++23 features used by the project.
# On macOS the user is expected to install a recent compiler via Homebrew.
if command -v g++-16 >/dev/null 2>&1; then
    CXX_COMPILER="g++-16"
elif command -v g++-15 >/dev/null 2>&1; then
    CXX_COMPILER="g++-15"
elif command -v clang++ >/dev/null 2>&1; then
    CXX_COMPILER="clang++"
else
    MISSING="${MISSING} C++ compiler"
    CXX_COMPILER=""
fi

if [[ -n "${MISSING}" ]]; then
    echo "ERROR: missing prerequisites:${MISSING}"
    echo "Install them first (see the CaptureMoment Build Guide), then re-run this script."
    exit 1
fi

# ==============================================================================
# System dependencies (Homebrew formula names)
# ==============================================================================
echo "[1/4] Installing system dependencies..."
brew install \
    ccache \
    spdlog curl brotli expat inih libssh \
    libxml2 zlib jpeg-turbo freetype libpng \
    libraw opencv jpeg-xl libheif pystring \
    libtiff imath openexr \
    yaml-cpp

echo "System dependencies installed."

# ==============================================================================
# Build and install Exiv2
# ==============================================================================
echo "[2/4] Building and installing Exiv2 ${EXIV2_VERSION}..."
if [[ -d "${INSTALL_PREFIX}/exiv2" ]]; then
    echo "Exiv2 already found in ${INSTALL_PREFIX}/exiv2, skipping."
else
    # Clean any leftovers from a previous interrupted run
    rm -rf /tmp/exiv2-src /tmp/exiv2-build

    git clone --depth 1 --branch "${EXIV2_VERSION}" \
        https://github.com/Exiv2/exiv2.git /tmp/exiv2-src

    cmake -S /tmp/exiv2-src -B /tmp/exiv2-build -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX="${INSTALL_PREFIX}/exiv2" \
        -DBUILD_SHARED_LIBS=ON \
        -DEXIV2_BUILD_SAMPLES=OFF \
        -DEXIV2_BUILD_TESTS=OFF

    cmake --build /tmp/exiv2-build --parallel "$(sysctl -n hw.ncpu)"
    sudo cmake --install /tmp/exiv2-build

    rm -rf /tmp/exiv2-src /tmp/exiv2-build
    echo "Exiv2 installed successfully in ${INSTALL_PREFIX}/exiv2."
fi

# ==============================================================================
# Build and install OpenImageIO
# ==============================================================================
echo "[3/4] Building and installing OpenImageIO ${OIIO_VERSION}..."
if [[ -d "${INSTALL_PREFIX}/oiio" ]]; then
    echo "OpenImageIO already found in ${INSTALL_PREFIX}/oiio, skipping."
else
    # Clean any leftovers from a previous interrupted run
    rm -rf /tmp/oiio-src /tmp/oiio-build

    git clone --depth 1 --branch "${OIIO_VERSION}" \
        https://github.com/AcademySoftwareFoundation/OpenImageIO.git /tmp/oiio-src

    cmake -S /tmp/oiio-src -B /tmp/oiio-build -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX="${INSTALL_PREFIX}/oiio" \
        -DEXIV2_ROOT="${INSTALL_PREFIX}/exiv2" \
        -DOpenImageIO_BUILD_MISSING_DEPS=required \
        -DSTOP_ON_WARNING=0 \
        -DOIIO_BUILD_TOOLS=OFF \
        -DOIIO_BUILD_TESTS=OFF \
        -DUSE_PYTHON=OFF

    cmake --build /tmp/oiio-build --parallel "$(sysctl -n hw.ncpu)"
    sudo cmake --install /tmp/oiio-build

    rm -rf /tmp/oiio-src /tmp/oiio-build
    echo "OpenImageIO installed successfully in ${INSTALL_PREFIX}/oiio."
fi

# ==============================================================================
# Install Halide (pip wheel, in a project-local virtualenv)
# ==============================================================================
echo "[4/4] Installing Halide ${HALIDE_VERSION} in ${VENV_DIR}..."

# The default python3 may be too old (asdf/system Python): pick the newest
# interpreter that satisfies the wheel requirement, installing one if needed.
PYTHON_BIN=""
for candidate in python3.15 python3.14 python3.13 python3.12 python3; do
    if command -v "${candidate}" >/dev/null 2>&1; then
        minor="$("${candidate}" -c 'import sys; print(sys.version_info[1])')"
        if (( minor >= PYTHON_MIN_MINOR )); then
            PYTHON_BIN="$(command -v "${candidate}")"
            break
        fi
    fi
done

if [[ -z "${PYTHON_BIN}" ]]; then
    echo "No Python 3.${PYTHON_MIN_MINOR}+ found, installing python@3.13 via Homebrew..."
    brew install python@3.13
    PYTHON_BIN="$(brew --prefix python@3.13)/bin/python3.13"
fi

echo "Using ${PYTHON_BIN} ($("${PYTHON_BIN}" --version))"

if [[ ! -x "${VENV_DIR}/bin/python" ]]; then
    "${PYTHON_BIN}" -m venv "${VENV_DIR}"
fi

"${VENV_DIR}/bin/python" -m pip install --quiet --upgrade pip
"${VENV_DIR}/bin/python" -m pip install --only-binary :all: "halide==${HALIDE_VERSION}"

HALIDE_PREFIX="$("${VENV_DIR}/bin/python" -c 'import halide; print(halide.install_dir())')"
echo "Halide installed successfully in ${HALIDE_PREFIX}."


echo ""
echo "Setup completed successfully! You can now build CaptureMoment."
echo "Next: source ${SHELL_RC}   (or open a new terminal)"
