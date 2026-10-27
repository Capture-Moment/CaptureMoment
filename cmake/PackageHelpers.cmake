# PackageHelpers.cmake - Helper functions to find and verify required packages

# ============================================================
# Download CPM.cmake automatically if not present
# ============================================================
if(NOT EXISTS "${CMAKE_BINARY_DIR}/cmake/CPM.cmake")

    message(STATUS "Downloading CPM.cmake...")
    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/cmake")
    file(DOWNLOAD
        https://github.com/cpm-cmake/CPM.cmake/releases/latest/download/get_cpm.cmake
        "${CMAKE_BINARY_DIR}/cmake/CPM.cmake"
        TLS_VERIFY ON
    )

endif()

include("${CMAKE_BINARY_DIR}/cmake/CPM.cmake")
message(STATUS "Using CPM.cmake from: ${CMAKE_BINARY_DIR}/cmake/CPM.cmake")

# ============================================================
# Find all required packages
# ============================================================
function(find_required_packages)

    # spdlog (mandatory)
    find_spdlog_package()

    # OpenImageIO (mandatory)
    find_openimageio_package()

    # Halide (mandatory)
    find_halide_package()

    # Exiv2 (mandatory for serialization)
    find_exiv2_package()

    # magic_enum (mandatory)
    find_magic_enum_package()

    if(BUILD_TESTS)
        find_gtest_package()
    endif()


    # Qt6 will be searched by the sub-projects ui/desktop, ui/mobile

    summarize_found_packages()
    warn_halide_requirements()

endfunction()


# ============================================================
# Find spdlog
# ============================================================
function(find_spdlog_package)

    message(STATUS "Searching for spdlog...")

    # Attempt CONFIG (vcpkg, Conan)
    find_package(spdlog CONFIG QUIET)

    if(NOT spdlog_FOUND)
        message(STATUS "spdlog not found via CONFIG, trying MODULE...")
        # Attempt MODULE (Findspdlog.cmake)
        find_package(spdlog MODULE QUIET)
    endif()
    
    if(spdlog_FOUND)
        set(spdlog_FOUND TRUE PARENT_SCOPE)
        set(spdlog_VERSION ${spdlog_VERSION} PARENT_SCOPE)
    else()
        message(FATAL_ERROR "spdlog not found. Please install it via your package manager or vcpkg/conan.")
    endif()
endfunction()

# ============================================================
# Find OpenImageIO
# ============================================================
function(find_openimageio_package)

    message(STATUS "Searching for OpenImageIO...")
    
    # Attempt CONFIG (vcpkg, Conan)
    find_package(OpenImageIO CONFIG QUIET)
    
    if(NOT OpenImageIO_FOUND)
        message(STATUS "OpenImageIO not found via CONFIG, trying MODULE...")
        # Attempt MODULE (FindOpenImageIO.cmake)
        find_package(OpenImageIO MODULE QUIET)
    endif()
    
    if(OpenImageIO_FOUND)
        set(OpenImageIO_FOUND TRUE PARENT_SCOPE)
        set(OpenImageIO_VERSION ${OpenImageIO_VERSION} PARENT_SCOPE)
    else()
        message(FATAL_ERROR "OpenImageIO not found. Please install it via your package manager or vcpkg/conan.")
    endif()
endfunction()

# ============================================================
# Find Halide
# ============================================================
function(find_halide_package)

    message(STATUS "Searching for Halide...")
    
    if(HALIDE_DIR)
    # Path to cmake Halide. -DHALIDE_DIR=/path/to/halide
    find_package(Halide CONFIG REQUIRED HINTS ${HALIDE_DIR})
    message(STATUS "Halide Found in this dir : ${HALIDE_DIR}")
    else()
    find_package(Halide CONFIG QUIET)
    endif()
    
    if(Halide_FOUND)        
        set(Halide_FOUND TRUE PARENT_SCOPE)
        set(Halide_VERSION ${Halide_VERSION} PARENT_SCOPE)
    else()
        message(FATAL_ERROR "Halide not found. Please install it via your package manager or vcpkg/conan.")
    endif()
endfunction()

# ============================================================
# Warning about Halide requirements
# ============================================================
function(warn_halide_requirements)
    message(STATUS "╔════════════════════════════════════════════════════════════╗")
    message(STATUS "║    Warning: Halide requires LLVM                           ║")
    message(STATUS "╠════════════════════════════════════════════════════════════╣")
    message(STATUS "║  If building from source (for GPU Support)                 ║")
    message(STATUS "║  - Space needed: 30-100 GB (debug or release)              ║")
    message(STATUS "║  - Build time: 30-240 minutes (depending on hardware)      ║")
    message(STATUS "╚════════════════════════════════════════════════════════════╝")
    message(STATUS "")
endfunction()

# ============================================================
# Find magic_enum
# ============================================================
function(find_magic_enum_package)

    message(STATUS "Fetching magic_enum v0.9.7 via CPM")

    CPMAddPackage(
        NAME magic_enum
        GITHUB_REPOSITORY Neargye/magic_enum
        GIT_TAG         v0.9.7
    )

    if(TARGET magic_enum::magic_enum)
        set(magic_enum_FOUND TRUE PARENT_SCOPE)
        set(magic_enum_VERSION "0.9.7" PARENT_SCOPE)
    else()
        message(FATAL_ERROR "Failed to download magic_enum via CPM.")
    endif()
endfunction()

# ============================================================
# Find Exiv2
# ============================================================
function(find_exiv2_package)

    message(STATUS "Searching for Exiv2...")

    # Attempt CONFIG (This looks for exiv2-config.cmake or Exiv2Config.cmake)
    # It will look in standard locations and also in CMAKE_PREFIX_PATH
    # The documentation suggests using NAMES exiv2
    find_package(exiv2 CONFIG REQUIRED NAMES exiv2)

    # The imported target name according to the documentation is Exiv2::exiv2lib
    if(TARGET Exiv2::exiv2lib)
        set(Exiv2_FOUND TRUE PARENT_SCOPE)
        set(exiv2_VERSION "${exiv2_VERSION}" PARENT_SCOPE)
    else()
        message(FATAL_ERROR "Exiv2 found but target Exiv2::exiv2lib is not available. Check installation.")
    endif()

endfunction()

#============================================================
# Find GTest
#============================================================
function(find_gtest_package)

    message(STATUS "Fetching GTest via CPM")
    CPMAddPackage(
      NAME GTest
      URL https://github.com/google/googletest/releases/download/v1.18.0/googletest-1.18.0.tar.gz
      VERSION 1.18.0
      OPTIONS
        "gtest_force_shared_crt ON"
        "INSTALL_GTEST OFF"
    )
    if(TARGET GTest::gtest)
      set(GTest_FOUND TRUE PARENT_SCOPE)
      set(GTest_VERSION "1.18.0" PARENT_SCOPE)
    else()
      message(FATAL_ERROR "GTest::gtest target not found after CPMAddPackage.")
    endif()

endfunction()
# ============================================================
# Summary of all found packages
# ============================================================
function(summarize_found_packages)
    
    message(STATUS "╔════════════════════════════════════════════════════════════╗")
    message(STATUS "║    Summary Package Information                             ║")
    message(STATUS "╠════════════════════════════════════════════════════════════╣")
    if(spdlog_FOUND)
        message(STATUS "║ spdlog : ${spdlog_VERSION}")
    else()
        message(STATUS "║ spdlog : Not Found")
    endif()

    if(OpenImageIO_FOUND)
        message(STATUS "║ OpenImageIO : ${OpenImageIO_VERSION}")
    else()
        message(STATUS "║ OpenImageIO : Not Found")
    endif()

    if(Halide_FOUND)
        message(STATUS "║ Halide : ${Halide_VERSION}")
    else()
        message(STATUS "║ Halide : Not Found")
    endif()

    if(Exiv2_FOUND)
        message(STATUS "║ Exiv2 : ${exiv2_VERSION}")
    else()
        message(STATUS "║ Exiv2 : Not Found")
    endif()

    if(magic_enum_FOUND)
        message(STATUS "║ magic_enum : ${magic_enum_VERSION}")
    else()
        message(STATUS "║ magic_enum : Not Found")
    endif()
    if(GTest_FOUND)
        message(STATUS "║ GTest : ${GTest_VERSION}")
    else()
        message(STATUS "║ GTest : Not Found or not needed (tests disabled)")
    endif()

    message(STATUS "╚════════════════════════════════════════════════════════════╝")
    message(STATUS "")

    message(STATUS "╔════════════════════════════════════════════════════════════╗")
    message(STATUS "║    Preferred Version                                       ║")
    message(STATUS "╠════════════════════════════════════════════════════════════╣")
    message(STATUS "║ spdlog : 1.16.0")
    message(STATUS "║ OpenImageIO : 3.1.14.0")
    message(STATUS "║ Halide : 21.0.0")
    message(STATUS "║ Exiv2 : 0.28.7")
    message(STATUS "║ magic_enum : 0.9.7")
    message(STATUS "║ GTest : 1.18.0")
    message(STATUS "╚════════════════════════════════════════════════════════════╝")
    message(STATUS "")
endfunction()