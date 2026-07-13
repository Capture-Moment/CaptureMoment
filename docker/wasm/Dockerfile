FROM ubuntu:26.10

ENV DEBIAN_FRONTEND=noninteractive

# ==============================================================================
# System dependencies
# ==============================================================================
RUN apt-get update && apt-get install -y \
    git ninja-build python3 python3-pip python3-venv ccache \
    libspdlog-dev libexpat1-dev libinih-dev \
    libxml2-utils zlib1g-dev libjpeg-dev libfreetype-dev \
    libpng-dev libuhd-dev libraw-dev \
    libjxl-dev libheif-dev libpystring-dev libtiff-dev libimath-dev \
    libyaml-cpp-dev \
    && rm -rf /var/lib/apt/lists/*

# ==============================================================================
# Install Python core packages
# ==============================================================================
RUN pip3 install --break-system-packages cmake aqtinstall

# ==============================================================================
# Install Emscripten SDK
# ==============================================================================
RUN git clone https://github.com/emscripten-core/emsdk.git /opt/emsdk && \
    cd /opt/emsdk && \
    ./emsdk install latest && \
    ./emsdk activate latest

# Activate Emscripten environment for all users
ENV PATH="/opt/emsdk:/opt/emsdk/upstream/emscripten:${PATH}"
ENV EMSDK="/opt/emsdk"

# ==============================================================================
# Install Qt framework for WebAssembly (wasm-singlethread)
# ==============================================================================
RUN aqt install-qt all_os wasm 6.10.2 wasm-singlethread --outputdir /opt/qt -m qtdeclarative -m qtshadertools --autodesktop --base https://download.qt.io/online/qtsdkrepository

# ==============================================================================
# Configuration for CMake
# ==============================================================================
ENV CMAKE_PREFIX_PATH="/opt/qt/6.10.2/wasm-singlethread;/opt/exiv2;/opt/oiio"

# ==============================================================================
# Build and install Exiv2 (WASM - STATIC)
# ==============================================================================
RUN git clone https://github.com/Exiv2/exiv2.git /tmp/exiv2-src && \
    cd /tmp/exiv2-src && git checkout v0.28.7 && \
    cmake -S . -B build -G Ninja \
          -DCMAKE_BUILD_TYPE=Release \
          -DCMAKE_INSTALL_PREFIX=/opt/exiv2 \
          -DBUILD_SHARED_LIBS=OFF \
          -DCMAKE_TOOLCHAIN_FILE=${EMSDK}/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake && \
    cmake --build build --parallel $(nproc) && \
    cmake --install build && \
    rm -rf /tmp/*

# ==============================================================================
# Build and install OpenImageIO (WASM - STATIC)
# ==============================================================================
RUN git clone https://github.com/AcademySoftwareFoundation/OpenImageIO.git /tmp/oiio-src && \
    cd /tmp/oiio-src && git checkout v3.1.14.0 && \
    cmake -S . -B build -G Ninja \
          -DCMAKE_BUILD_TYPE=Release \
          -DCMAKE_INSTALL_PREFIX=/opt/oiio \
          -DBUILD_SHARED_LIBS=OFF \
          -DCMAKE_TOOLCHAIN_FILE=${EMSDK}/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake \
          -DOpenImageIO_BUILD_MISSING_DEPS=required \
          -DSTOP_ON_WARNING=0 \
          -DOIIO_BUILD_TOOLS=OFF \
          -DOIIO_BUILD_TESTS=OFF \
          -DUSE_PYTHON=OFF \
          -DUSE_FFMPEG=OFF && \
    cmake --build build --parallel $(nproc) && \
    cmake --install build && \
    rm -rf /tmp/*

WORKDIR /app