FROM gcc:16.1.0

ENV DEBIAN_FRONTEND=noninteractive

# ==============================================================================
# System dependencies
# ==============================================================================
RUN apt-get update && apt-get install -y \
    ninja-build python3 python3-pip python3-venv ccache \
    patchelf desktop-file-utils \
    libxcb-cursor0 \
    libspdlog-dev libcurl4-openssl-dev libxkbcommon-dev libgl1-mesa-dev \
    libbrotli-dev libexpat1-dev libinih-dev libssh-dev \
    libxml2-utils libz-dev zlib1g-dev libjpeg-dev libfreetype-dev \
    libpng-dev libuhd-dev libraw-dev libopencv-core-dev libopencv-imgproc-dev \
    libjxl-dev libheif-dev libpystring-dev libtiff-dev libimath-dev \
    libopenexr-dev \
    libyaml-cpp-dev \
    && rm -rf /var/lib/apt/lists/*

# ==============================================================================
# AppImage packaging tools
# ==============================================================================
RUN mkdir -p /opt/tools && cd /opt/tools && \
    wget -q --max-redirect=5 \
      https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage && \
    wget -q --max-redirect=5 \
      https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage && \
    chmod +x linuxdeploy*.AppImage

# ==============================================================================
# Install Python core packages
# ==============================================================================
RUN pip3 install --break-system-packages cmake aqtinstall
RUN pip3 install --break-system-packages --only-binary :all: halide==21.0.0

# ==============================================================================
# Install Qt framework
# ==============================================================================
RUN aqt install-qt linux desktop 6.10.2 linux_gcc_64 --outputdir /opt/qt -m qtshadertools

# ==============================================================================
# Configuration for CMake and library paths
# ==============================================================================
ENV Qt6_DIR=/opt/qt/6.10.2/gcc_64/lib/cmake/Qt6
ENV CMAKE_PREFIX_PATH="/opt/qt/6.10.2/gcc_64;/opt/exiv2;/opt/oiio"
ENV LD_LIBRARY_PATH="/opt/exiv2/lib:/opt/oiio/lib"
ENV PATH="/opt/qt/6.10.2/gcc_64/bin:${PATH}"

# ==============================================================================
# Build and install Exiv2
# ==============================================================================
RUN git clone https://github.com/Exiv2/exiv2.git /tmp/exiv2-src && \
    cd /tmp/exiv2-src && git checkout v0.28.7 && \
    cmake -S . -B build -G Ninja \
          -DCMAKE_BUILD_TYPE=Release \
          -DCMAKE_INSTALL_PREFIX=/opt/exiv2 \
          -DBUILD_SHARED_LIBS=ON && \
    cmake --build build --parallel $(nproc) && \
    cmake --install build && \
    rm -rf /tmp/*

# ==============================================================================
# Build and install OpenImageIO
# ==============================================================================
RUN git clone https://github.com/AcademySoftwareFoundation/OpenImageIO.git /tmp/oiio-src && \
    cd /tmp/oiio-src && git checkout v3.1.14.0 && \
    cmake -S . -B build -G Ninja \
           -DCMAKE_BUILD_TYPE=Release \
           -DCMAKE_INSTALL_PREFIX=/opt/oiio \
           -DOpenImageIO_BUILD_MISSING_DEPS=required \
           -DSTOP_ON_WARNING=0 \
           -DOIIO_BUILD_TOOLS=OFF \
           -DOIIO_BUILD_TESTS=OFF \
           -DUSE_PYTHON=OFF &&\
    cmake --build build --parallel $(nproc) && \
    cmake --install build && \
    rm -rf /tmp/*
WORKDIR /app


# TMP
RUN test "$(gcc -print-file-name=libasan.so)" != "libasan.so" && \
    test "$(gcc -print-file-name=libubsan.so)" != "libubsan.so" && \
    echo "libasan et libubsan OK"