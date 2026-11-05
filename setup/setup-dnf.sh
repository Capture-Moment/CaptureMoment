#!/bin/bash

# ==============================================================================
# CaptureMoment - Linux Local Setup Script (Fedora / RPM-based)
#
# Automates the setup of the build environment and compiles the required core
# dependencies (Exiv2, OpenImageIO) from source.
#
# Works on Fedora and other dnf-based distributions (RHEL, CentOS Stream,
# Rocky, Alma) — recent releases only, where the -devel packages below exist.
# ==============================================================================

set -e

EXIV2_VERSION="v0.28.7"
OIIO_VERSION="v3.1.14.0"
INSTALL_PREFIX="/opt"

echo "CaptureMoment - Linux setup (dnf/RPM)"
echo "Exiv2: ${INSTALL_PREFIX}/exiv2"
echo "OpenImageIO: ${INSTALL_PREFIX}/oiio"
BASHRC="$HOME/.bashrc"

# ==============================================================================
# Prerequisites check (not installed by this script - user responsibility)
# ==============================================================================
MISSING=""
command -v cmake >/dev/null 2>&1 || MISSING="${MISSING} cmake(4.2+)"
command -v ninja >/dev/null 2>&1 || MISSING="${MISSING} ninja-build"
command -v python3 >/dev/null 2>&1 || MISSING="${MISSING} python"
command -v git >/dev/null 2>&1 || MISSING="${MISSING} git"

GCC_MIN=16
if ! command -v g++ >/dev/null 2>&1 || (( $(g++ -dumpversion | cut -d. -f1) < GCC_MIN )); then
    MISSING="${MISSING} gcc(${GCC_MIN}+: please upgrade to GCC ${GCC_MIN} for C++23)"
fi

if [[ -n "${MISSING}" ]]; then
    echo "ERROR: missing prerequisites:${MISSING}"
    echo "Install them first (see the CaptureMoment Build Guide), then re-run this script."
    exit 1
fi

# ==============================================================================
# System dependencies (Fedora package names)
# ==============================================================================
echo "[1/4] Installing system dependencies..."
sudo dnf install -y \
    ccache \
    patchelf desktop-file-utils \
    xcb-util-cursor \
    spdlog-devel libcurl-devel libxkbcommon-devel mesa-libGL-devel \
    brotli-devel expat-devel inih-devel libssh-devel \
    libxml2-devel zlib-devel libjpeg-turbo-devel freetype-devel \
    libpng-devel uhd-devel LibRaw-devel opencv-devel \
    libjxl-devel libheif-devel pystring-devel libtiff-devel imath-devel \
    OpenEXR-devel \
    yaml-cpp-devel

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

    cmake --build /tmp/exiv2-build --parallel "$(nproc)"
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

    cmake --build /tmp/oiio-build --parallel "$(nproc)"
    sudo cmake --install /tmp/oiio-build

    rm -rf /tmp/oiio-src /tmp/oiio-build
    echo "OpenImageIO installed successfully in ${INSTALL_PREFIX}/oiio."
fi

# ==============================================================================
# Environment variables (persisted to ~/.bashrc if not already present)
# ==============================================================================
echo "[4/4] Configuring environment variables..."

EXIV2_ENV='export CMAKE_PREFIX_PATH="/opt/exiv2:/opt/oiio:${CMAKE_PREFIX_PATH}"'
OIIO_ENV='export LD_LIBRARY_PATH="/opt/exiv2/lib:/opt/oiio/lib:${LD_LIBRARY_PATH}"'

if ! grep -qF 'CMAKE_PREFIX_PATH="/opt/exiv2' "${BASHRC}"; then
    printf '\n# CaptureMoment build environment\n%s\n%s\n' "${EXIV2_ENV}" "${OIIO_ENV}" >> "${BASHRC}"
    echo "Added CaptureMoment environment lines to ${BASHRC}."
else
    echo "Environment lines already present in ${BASHRC}, skipping."
fi

# ==============================================================================
# Configuration for CMake and library paths
# ==============================================================================
echo ""
echo "Configuring environment variables..."
export CMAKE_PREFIX_PATH="/opt/exiv2:/opt/oiio:${CMAKE_PREFIX_PATH}"
export LD_LIBRARY_PATH="/opt/exiv2/lib:/opt/oiio/lib:${LD_LIBRARY_PATH}"

echo ""
echo "Setup completed successfully! You can now build CaptureMoment."
