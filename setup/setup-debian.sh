#!/bin/bash

# ==============================================================================
# CaptureMoment - Linux Local Setup Script
#
# Automates the setup of the build environment, installs Halide in a project
# virtualenv and compiles the required core dependencies (Exiv2, OpenImageIO)
# from source.
# ==============================================================================

set -e

EXIV2_VERSION="v0.28.7"
OIIO_VERSION="v3.1.14.0"
HALIDE_VERSION="22.1.0"
INSTALL_PREFIX="/opt"

# Halide ships as a pip wheel, installed in a virtualenv at the repo root so it
# stays isolated from the system Python. CMake picks it up automatically.
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VENV_DIR="${REPO_ROOT}/.venv"
# The Halide wheels are cp312-abi3: Python 3.12+ is required.
PYTHON_MIN_MINOR=12

echo "CaptureMoment - Linux setup"
echo "Exiv2: ${INSTALL_PREFIX}/exiv2"
echo "OpenImageIO: ${INSTALL_PREFIX}/oiio"
echo "Halide ${HALIDE_VERSION}: ${VENV_DIR}"
BASHRC="$HOME/.bashrc"

# ==============================================================================
# Prerequisites check (not installed by this script - user responsibility)
# ==============================================================================
MISSING=""
command -v cmake >/dev/null 2>&1 || MISSING="${MISSING} cmake(4.2+)"
command -v ninja >/dev/null 2>&1 || MISSING="${MISSING} ninja"
command -v python3 >/dev/null 2>&1 || MISSING="${MISSING} python"
command -v git >/dev/null 2>&1 || MISSING="${MISSING} git"

GCC_MIN=16
if ! command -v g++-${GCC_MIN} >/dev/null 2>&1; then
    if ! command -v g++ >/dev/null 2>&1 || (( $(g++ -dumpversion | cut -d. -f1) < GCC_MIN )); then
        MISSING="${MISSING} gcc(${GCC_MIN}+: please upgrade to GCC ${GCC_MIN} for C++23)"
    fi
fi

if [[ -n "${MISSING}" ]]; then
    echo "ERROR: missing prerequisites:${MISSING}"
    exit 1
fi

if [[ -n "${MISSING}" ]]; then
    echo "ERROR: missing prerequisites:${MISSING}"
    echo "Install them first (see the CaptureMoment Build Guide), then re-run this script."
    exit 1
fi

# ==============================================================================
# System dependencies
# ==============================================================================
echo "[1/5] Installing system dependencies..."
sudo apt-get update && sudo apt-get install -y \
    ccache \
    patchelf desktop-file-utils \
    libxcb-cursor0 \
    libspdlog-dev libcurl4-openssl-dev libxkbcommon-dev libgl1-mesa-dev \
    libbrotli-dev libexpat1-dev libinih-dev libssh-dev \
    libxml2-utils zlib1g-dev libjpeg-dev libfreetype-dev \
    libpng-dev libuhd-dev libraw-dev libopencv-core-dev libopencv-imgproc-dev \
    libjxl-dev libheif-dev libpystring-dev libtiff-dev libimath-dev libjpeg-turbo-progs \
    libopenexr-dev \
    libyaml-cpp-dev \
    python3-venv

echo "System dependencies installed."
# ==============================================================================
# Build and install Exiv2
# ==============================================================================
echo "[2/5] Building and installing Exiv2 ${EXIV2_VERSION}..."
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

    cmake --build /tmp/exiv2-build --parallel "$(nproc)"
    sudo cmake --install /tmp/exiv2-build

    rm -rf /tmp/exiv2-src /tmp/exiv2-build
    echo "Exiv2 installed successfully in ${INSTALL_PREFIX}/exiv2."
fi

# ==============================================================================
# Build and install OpenImageIO
# ==============================================================================
echo "[3/5] Building and installing OpenImageIO ${OIIO_VERSION}..."
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

    cmake --build /tmp/oiio-build --parallel "$(nproc)"
    sudo cmake --install /tmp/oiio-build

    rm -rf /tmp/oiio-src /tmp/oiio-build
    echo "OpenImageIO installed successfully in ${INSTALL_PREFIX}/oiio."
fi


# ==============================================================================
# Install Halide (pip wheel, in a project-local virtualenv)
# ==============================================================================
echo "[4/5] Installing Halide ${HALIDE_VERSION} in ${VENV_DIR}..."

# The distro's default python3 may be too old: pick the newest interpreter that
# satisfies the wheel requirement, installing one if needed.
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
    echo "No Python 3.${PYTHON_MIN_MINOR}+ found, trying to install it..."
    sudo apt-get install -y python3.12 python3.12-venv || true
    if command -v python3.12 >/dev/null 2>&1; then
        PYTHON_BIN="$(command -v python3.12)"
    else
        echo "ERROR: Halide ${HALIDE_VERSION} requires Python 3.${PYTHON_MIN_MINOR}+ (the wheels are cp312-abi3)."
        echo "Install a recent Python, then re-run this script."
        exit 1
    fi
fi

echo "Using ${PYTHON_BIN} ($("${PYTHON_BIN}" --version))"

if [[ ! -x "${VENV_DIR}/bin/python" ]]; then
    "${PYTHON_BIN}" -m venv "${VENV_DIR}"
fi

"${VENV_DIR}/bin/python" -m pip install --quiet --upgrade pip
"${VENV_DIR}/bin/python" -m pip install --only-binary :all: "halide==${HALIDE_VERSION}"

HALIDE_PREFIX="$("${VENV_DIR}/bin/python" -c 'import halide; print(halide.install_dir())')"
echo "Halide installed successfully in ${HALIDE_PREFIX}."

# ==============================================================================
# Environment variables (persisted to ~/.bashrc if not already present)
# ==============================================================================
echo "[5/5] Configuring environment variables..."

EXIV2_ENV='export CMAKE_PREFIX_PATH="/opt/exiv2:/opt/oiio:${CMAKE_PREFIX_PATH}"'
OIIO_ENV='export LD_LIBRARY_PATH="/opt/exiv2/lib:/opt/oiio/lib:${LD_LIBRARY_PATH}"'
# Resolved here because the path contains the venv's Python minor version.
HALIDE_ENV="export LD_LIBRARY_PATH=\"${HALIDE_PREFIX}/lib:\${LD_LIBRARY_PATH}\""

if ! grep -qF 'CMAKE_PREFIX_PATH="/opt/exiv2' "${BASHRC}"; then
    printf '\n# CaptureMoment build environment\n%s\n%s\n' "${EXIV2_ENV}" "${OIIO_ENV}" >> "${BASHRC}"
    echo "Added CaptureMoment environment lines to ${BASHRC}."
else
    echo "Environment lines already present in ${BASHRC}, skipping."
fi

if ! grep -qF "${HALIDE_PREFIX}/lib" "${BASHRC}"; then
    printf '%s\n' "${HALIDE_ENV}" >> "${BASHRC}"
    echo "Added Halide library path to ${BASHRC}."
else
    echo "Halide library path already present in ${BASHRC}, skipping."
fi

# ==============================================================================
# Configuration for CMake and library paths
# ==============================================================================
echo ""
echo "Configuring environment variables..."
export CMAKE_PREFIX_PATH="/opt/exiv2:/opt/oiio:${CMAKE_PREFIX_PATH}"
export LD_LIBRARY_PATH="/opt/exiv2/lib:/opt/oiio/lib:${HALIDE_PREFIX}/lib:${LD_LIBRARY_PATH}"

echo "To make these variables persistent, add the following lines to your ~/.bashrc:"
echo "export CMAKE_PREFIX_PATH=\"/opt/exiv2:/opt/oiio:\${CMAKE_PREFIX_PATH}\""
echo "export LD_LIBRARY_PATH=\"/opt/exiv2/lib:/opt/oiio/lib:${HALIDE_PREFIX}/lib:\${LD_LIBRARY_PATH}\""
echo ""
echo "Setup completed successfully! You can now build CaptureMoment."
