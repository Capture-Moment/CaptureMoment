FROM ubuntu:24.04

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

RUN emcc --version

# ==============================================================================
# Install Qt framework for WebAssembly (wasm-multithread)
# ==============================================================================

RUN aqt install-qt all_os wasm 6.10.2 wasm_multithread --outputdir /opt/qt -m qtdeclarative -m qtshadertools --autodesktop

# ==============================================================================
# Configuration for CMake
# ==============================================================================
ENV CMAKE_PREFIX_PATH="/opt/qt/6.10.2/wasm_multithread;/opt/exiv2;/opt/oiio"

# ==============================================================================
# Build and install ZLIB for WebAssembly
# ==============================================================================
RUN git clone https://github.com/madler/zlib.git /tmp/zlib-src && \
    cd /tmp/zlib-src && \
    emcmake cmake -S . -B build -G Ninja \
          -DCMAKE_BUILD_TYPE=Release \
          -DCMAKE_INSTALL_PREFIX=/opt/zlib-wasm && \
    cmake --build build --parallel $(nproc) && \
    cmake --install build && \
    rm -rf /tmp/*

# ==============================================================================
# Build and install Expat for WebAssembly
# ==============================================================================
RUN git clone https://github.com/libexpat/libexpat.git /tmp/expat-src && \
    cd /tmp/expat-src && \
    emcmake cmake -S expat -B build -G Ninja \
          -DCMAKE_BUILD_TYPE=Release \
          -DCMAKE_INSTALL_PREFIX=/opt/expat-wasm \
          -DBUILD_SHARED_LIBS=OFF \
          -DEXPAT_BUILD_EXAMPLES=OFF \
          -DEXPAT_BUILD_TESTS=OFF \
          -DEXPAT_BUILD_TOOLS=OFF && \
    cmake --build build --parallel $(nproc) && \
    cmake --install build && \
    rm -rf /tmp/*

# ==============================================================================
# Build and install inih for WebAssembly (Manual build)
# ==============================================================================
RUN git clone https://github.com/benhoyt/inih.git /tmp/inih-src && \
    cd /tmp/inih-src && \
    mkdir -p /opt/inih-wasm/include /opt/inih-wasm/lib && \
    cp ini.h /opt/inih-wasm/include/ && \
    cp cpp/INIReader.h /opt/inih-wasm/include/ && \
    em++ -O2 -c ini.c -o ini.o && \
    em++ -O2 -c cpp/INIReader.cpp -o INIReader.o -I. && \
    emar rcs /opt/inih-wasm/lib/libinih.a ini.o INIReader.o && \
    rm -rf /tmp/*

# ==============================================================================
# Build and install Exiv2 (WASM - STATIC)
# ==============================================================================
RUN git clone https://github.com/Exiv2/exiv2.git /tmp/exiv2-src && \
    cd /tmp/exiv2-src && git checkout v0.28.7 && \
    emcmake cmake -S . -B build -G Ninja \
          -DCMAKE_BUILD_TYPE=Release \
          -DCMAKE_INSTALL_PREFIX=/opt/exiv2 \
          -DBUILD_SHARED_LIBS=OFF \
          -DZLIB_INCLUDE_DIR=/opt/zlib-wasm/include \
          -DZLIB_LIBRARY=/opt/zlib-wasm/lib/libz.a \
          -DEXPAT_INCLUDE_DIR=/opt/expat-wasm/include \
          -DEXPAT_LIBRARY=/opt/expat-wasm/lib/libexpat.a \
          -Dinih_INCLUDE_DIR=/opt/inih-wasm/include \
          -Dinih_LIBRARY=/opt/inih-wasm/lib/libinih.a \
          -Dinih_inireader_INCLUDE_DIR=/opt/inih-wasm/include \
          -Dinih_inireader_LIBRARY=/opt/inih-wasm/lib/libinih.a \
          -DEXIV2_ENABLE_BROTLI=OFF \
          -DEXIV2_ENABLE_CURL=OFF \
          -DEXIV2_ENABLE_SSH=OFF \
          -DEXIV2_ENABLE_WEBREADY=OFF \
          -DEXIV2_BUILD_SAMPLES=OFF \
          -DEXIV2_BUILD_EXIV2_COMMAND=OFF && \
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