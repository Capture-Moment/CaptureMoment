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
RUN pip3 install --break-system-packages cmake aqtinstall halide

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
# Install Qt framework (wasm-multithread)
# ==============================================================================

RUN aqt install-qt all_os wasm 6.10.2 wasm_multithread --outputdir /opt/qt -m qtdeclarative -m qtshadertools --autodesktop

# ==============================================================================
# Build and install spdlog (Header-Only)
# ==============================================================================
RUN git clone https://github.com/gabime/spdlog.git /tmp/spdlog-src && \
    cd /tmp/spdlog-src && git checkout v1.16.0 && \
    emcmake cmake -S . -B build -G Ninja \
          -DCMAKE_BUILD_TYPE=Release \
          -DCMAKE_INSTALL_PREFIX=/opt/spdlog-wasm \
          -DSPDLOG_HEADER_ONLY=ON && \
    cmake --build build --parallel $(nproc) && \
    cmake --install build && \
    rm -rf /tmp/*


# ==============================================================================
# Build and install ZLIB for Exiv2 and OpenImageIO require ZLIB to be built and installed for WebAssembly since it does not provide a CMake build system.
# ==============================================================================
RUN git clone https://github.com/madler/zlib.git /tmp/zlib-src && \
    cd /tmp/zlib-src && \
    emcmake cmake -S . -B build -G Ninja \
          -DCMAKE_BUILD_TYPE=Release \
          -DCMAKE_INSTALL_PREFIX=/opt/zlib-wasm \
          -DCMAKE_POSITION_INDEPENDENT_CODE=ON && \
    cmake --build build --parallel $(nproc) && \
    cmake --install build && \
    rm -rf /tmp/*


# ======================== Exiv2 Dependencies for WebAssembly =========================

# ==============================================================================
# Build and install Expat for WebAssembly Exiv2 requires Expat to be built and installed for WebAssembly since it does not provide a CMake build system.
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
# Build and install inih for WebAssembly (Manual build) Exiv2 requires inih to be built and installed manually for WebAssembly since it does not provide a CMake build system.
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

# ======================== OpenColorIO Dependencies =========================

# ==============================================================================
# Build and install pystring
# ==============================================================================

RUN git clone https://github.com/imageworks/pystring.git /tmp/pystring-src && \
    cd /tmp/pystring-src && \
    emcmake cmake -S . -B build -G Ninja \
          -DCMAKE_BUILD_TYPE=Release \
          -DCMAKE_INSTALL_PREFIX=/opt/pystring-wasm \
          -DBUILD_SHARED_LIBS=OFF && \
    cmake --build build --parallel $(nproc) && \
    cmake --install build && \
    rm -rf /tmp/*

# ==============================================================================
# Build and install yaml-cpp
# ==============================================================================
RUN git clone https://github.com/jbeder/yaml-cpp.git /tmp/yaml-cpp-src && \
    cd /tmp/yaml-cpp-src && \
    emcmake cmake -S . -B build -G Ninja \
          -DCMAKE_BUILD_TYPE=Release \
          -DCMAKE_INSTALL_PREFIX=/opt/yaml-cpp-wasm \
          -DBUILD_SHARED_LIBS=OFF \
          -DYAML_CPP_BUILD_TESTS=OFF && \
    cmake --build build --parallel $(nproc) && \
    cmake --install build && \
    rm -rf /tmp/*

# ==============================================================================
# Build and install minizip (Required by OpenColorIO)
# ==============================================================================
RUN git clone https://github.com/madler/zlib.git /tmp/minizip-src && \
    cd /tmp/minizip-src && \
    cd contrib/minizip && \
    emcmake cmake -S . -B build -G Ninja \
          -DCMAKE_BUILD_TYPE=Release \
          -DCMAKE_INSTALL_PREFIX=/opt/minizip-wasm \
          -DBUILD_SHARED_LIBS=OFF \
          -DZLIB_INCLUDE_DIR=/opt/zlib-wasm/include \
          -DZLIB_LIBRARY=/opt/zlib-wasm/lib/libz.a && \
    cmake --build build --parallel $(nproc) && \
    cmake --install build && \
    rm -rf /tmp/*

# ==============================================================================
# Build and install OpenColorIO
# ==============================================================================
RUN git clone https://github.com/AcademySoftwareFoundation/OpenColorIO.git /tmp/opencolorio-src && \
    cd /tmp/opencolorio-src && \
    emcmake cmake -S . -B build -G Ninja \
          -DCMAKE_BUILD_TYPE=Release \
          -DCMAKE_INSTALL_PREFIX=/opt/opencolorio-wasm \
          -DBUILD_SHARED_LIBS=OFF \
          -DBUILD_TESTING=OFF \
          -DOCIO_BUILD_APPS=OFF \
          -DOCIO_BUILD_DOCS=OFF \
          -DOCIO_BUILD_PYTHON=OFF \
          -DOCIO_BUILD_JNIGLUE=OFF \
          -Dexpat_DIR=/opt/expat-wasm/lib/cmake/expat-2.6.4 \
          -Dyaml-cpp_DIR=/opt/yaml-cpp-wasm/lib/cmake/yaml-cpp \
          -Dpystring_DIR=/opt/pystring-wasm/lib/cmake/pystring \
          -DImath_DIR=/opt/imath-wasm/lib/cmake/Imath \
          -DMINIZIP_INCLUDE_DIR=/opt/minizip-wasm/include \
          -DMINIZIP_LIBRARY=/opt/minizip-wasm/lib/libminizip.a && \
    cmake --build build --parallel $(nproc) && \
    cmake --install build && \
    rm -rf /tmp/*

# ==============================================================================
# Build and install fmtlib
# ==============================================================================
RUN git clone https://github.com/fmtlib/fmt.git /tmp/fmt-src && \
    cd /tmp/fmt-src && git checkout 10.2.1 && \
    emcmake cmake -S . -B build -G Ninja \
          -DCMAKE_BUILD_TYPE=Release \
          -DCMAKE_INSTALL_PREFIX=/opt/fmt-wasm \
          -DBUILD_SHARED_LIBS=OFF \
          -DFMT_TEST=OFF && \
    cmake --build build --parallel $(nproc) && \
    cmake --install build && \
    rm -rf /tmp/*

# ======================== OpenImageIO Dependencies =========================

# ==============================================================================
# Build and install Imath requires Imath to be built and installed for WebAssembly since it does not provide a CMake build system.
# ==============================================================================
RUN git clone https://github.com/AcademySoftwareFoundation/Imath.git /tmp/imath-src && \
    cd /tmp/imath-src && \
    emcmake cmake -S . -B build -G Ninja \
          -DCMAKE_BUILD_TYPE=Release \
          -DCMAKE_INSTALL_PREFIX=/opt/imath-wasm \
          -DBUILD_SHARED_LIBS=OFF \
          -DBUILD_TESTING=OFF && \
    cmake --build build --parallel $(nproc) && \
    cmake --install build && \
    rm -rf /tmp/*

# ==============================================================================
# Build and install OpenEXR requires OpenEXR to be built and installed for WebAssembly since it does not provide a CMake build system.
# ==============================================================================
RUN git clone https://github.com/AcademySoftwareFoundation/openexr.git /tmp/openexr-src && \
    cd /tmp/openexr-src && \
    emcmake cmake -S . -B build -G Ninja \
          -DCMAKE_BUILD_TYPE=Release \
          -DCMAKE_INSTALL_PREFIX=/opt/openexr-wasm \
          -DBUILD_SHARED_LIBS=OFF \
          -DBUILD_TESTING=OFF \
          -DOPENEXR_BUILD_EXAMPLES=OFF \
          -DOPENEXR_BUILD_TOOLS=OFF \
          -DImath_DIR=/opt/imath-wasm/lib/cmake/Imath \
          -DZLIB_INCLUDE_DIR=/opt/zlib-wasm/include \
          -DZLIB_LIBRARY=/opt/zlib-wasm/lib/libz.a && \
    cmake --build build --parallel $(nproc) && \
    cmake --install build && \
    rm -rf /tmp/*

# ==============================================================================
# Build and install LibJPEG-Turbo
# ==============================================================================
RUN git clone https://github.com/libjpeg-turbo/libjpeg-turbo.git /tmp/libjpeg-src && \
    cd /tmp/libjpeg-src && \
    emcmake cmake -S . -B build -G Ninja \
          -DCMAKE_BUILD_TYPE=Release \
          -DCMAKE_INSTALL_PREFIX=/opt/libjpeg-wasm \
          -DENABLE_SHARED=OFF \
          -DWITH_SIMD=OFF && \
    cmake --build build --parallel $(nproc) && \
    cmake --install build && \
    rm -rf /tmp/*
# ==============================================================================
# Build and install LibPNG
# ==============================================================================
RUN git clone https://github.com/pnggroup/libpng.git /tmp/libpng-src && \
    cd /tmp/libpng-src && \
    emcmake cmake -S . -B build -G Ninja \
          -DCMAKE_BUILD_TYPE=Release \
          -DCMAKE_INSTALL_PREFIX=/opt/libpng-wasm \
          -DPNG_SHARED=OFF \
          -DPNG_TESTS=OFF \
          -DZLIB_INCLUDE_DIR=/opt/zlib-wasm/include \
          -DZLIB_LIBRARY=/opt/zlib-wasm/lib/libz.a && \
    cmake --build build --parallel $(nproc) && \
    cmake --install build && \
    rm -rf /tmp/*

# ==============================================================================
# Build and install LibTIFF
# ==============================================================================
RUN git clone https://gitlab.com/libtiff/libtiff.git /tmp/libtiff-src && \
    cd /tmp/libtiff-src &&  \
    emcmake cmake -S . -B build -G Ninja \
          -DCMAKE_BUILD_TYPE=Release \
          -DCMAKE_INSTALL_PREFIX=/opt/libtiff-wasm \
          -DBUILD_SHARED_LIBS=OFF \
          -Dtiff-tests=OFF \
          -DZLIB_INCLUDE_DIR=/opt/zlib-wasm/include \
          -DZLIB_LIBRARY=/opt/zlib-wasm/lib/libz.a \
          -DJPEG_INCLUDE_DIR=/opt/libjpeg-wasm/include \
          -DJPEG_LIBRARY=/opt/libjpeg-wasm/lib/libjpeg.a && \
    cmake --build build --parallel $(nproc) && \
    cmake --install build && \
    rm -rf /tmp/*

# ==============================================================================
# Build and install OpenImageIO (WASM - STATIC)
# ==============================================================================
RUN git clone https://github.com/AcademySoftwareFoundation/OpenImageIO.git /tmp/oiio-src && \
    cd /tmp/oiio-src && git checkout v3.1.14.0 && \
    emcmake cmake -S . -B build -G Ninja \
          -DCMAKE_BUILD_TYPE=Release \
          -DCMAKE_INSTALL_PREFIX=/opt/oiio \
          -DBUILD_SHARED_LIBS=OFF \
          -DCMAKE_CXX_COMPILER_VERSION=22.1.0 \
          -DOpenImageIO_BUILD_MISSING_DEPS=required \
          -DSTOP_ON_WARNING=0 \
          -DUSE_ZLIB=ON \
          -DZLIB_INCLUDE_DIR=/opt/zlib-wasm/include \
          -DZLIB_LIBRARY=/opt/zlib-wasm/lib/libz.a \
          -DImath_DIR=/opt/imath-wasm/lib/cmake/Imath \
          -DOpenEXR_DIR=/opt/openexr-wasm/lib/cmake/OpenEXR \
          -DOpenColorIO_DIR=/opt/opencolorio-wasm/lib/cmake/OpenColorIO \
          -Dfmt_DIR=/opt/fmt-wasm/lib/cmake/fmt \
          -Dlibjpeg-turbo_DIR=/opt/libjpeg-wasm/lib/cmake/libjpeg-turbo \
          -DJPEG_INCLUDE_DIR=/opt/libjpeg-wasm/include \
          -DJPEG_LIBRARY=/opt/libjpeg-wasm/lib/libjpeg.a \
          -DPNG_PNG_INCLUDE_DIR=/opt/libpng-wasm/include/libpng16 \
          -DPNG_LIBRARY=/opt/libpng-wasm/lib/libpng16.a \
          -DTIFF_INCLUDE_DIR=/opt/libtiff-wasm/include \
          -DTIFF_LIBRARY=/opt/libtiff-wasm/lib/libtiff.a \
          -DOIIO_BUILD_TOOLS=OFF \
          -DOIIO_BUILD_TESTS=OFF \
          -DUSE_PYTHON=OFF \
          -DUSE_FFMPEG=OFF && \
    cmake --build build --parallel $(nproc) && \
    cmake --install build && \
    rm -rf /tmp/*

WORKDIR /app

# ==============================================================================
# Configuration for CMake
# ==============================================================================
ENV CMAKE_PREFIX_PATH="/opt/qt/6.10.2/wasm_multithread;/opt/exiv2;/opt/oiio;/opt/spdlog-wasm"