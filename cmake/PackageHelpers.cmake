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

endfunction()


# ============================================================
# Find spdlog (header-only, via CPM)
# ============================================================
function(find_spdlog_package)

    message(STATUS "Fetching spdlog v1.17.0 (header-only) via CPM")

    CPMAddPackage(
        NAME spdlog
        GITHUB_REPOSITORY gabime/spdlog
        GIT_TAG v1.17.0
        OPTIONS
            "SPDLOG_BUILD_EXAMPLE OFF"
            "SPDLOG_BUILD_TESTS OFF"
            "SPDLOG_BUILD_BENCH OFF"
            "SPDLOG_INSTALL OFF"
            "SPDLOG_FMT_EXTERNAL OFF"
            "SPDLOG_USE_STD_FORMAT ON"
    )

    if(NOT TARGET spdlog::spdlog_header_only)
        message(FATAL_ERROR "spdlog::spdlog_header_only target not found after CPMAddPackage.")
    endif()

    set(spdlog_FOUND TRUE PARENT_SCOPE)
    set(spdlog_VERSION "1.17.0" PARENT_SCOPE)
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
#
# Halide is distributed as a pip wheel (halide / halide-bin /
# halide-runtime). The setup scripts install it in a project-local
# virtualenv, so look there first, then fall back to an explicit
# HALIDE_PREFIX, an active virtualenv, or a system-wide install.
# ============================================================
function(find_halide_package)

    message(STATUS "Searching for Halide...")

    # Explicit override wins (exported by the setup scripts / CI)
    if(DEFINED ENV{HALIDE_PREFIX})
        list(APPEND CMAKE_PREFIX_PATH "$ENV{HALIDE_PREFIX}")
    endif()

    # Ask the virtualenv's Python where the wheel installed Halide.
    # Covers both the project-local .venv and any activated virtualenv.
    foreach(_venv "${CMAKE_SOURCE_DIR}/.venv" "$ENV{VIRTUAL_ENV}")
        if(EXISTS "${_venv}/bin/python")
            execute_process(
                COMMAND "${_venv}/bin/python" -c "import halide; print(halide.install_dir())"
                OUTPUT_VARIABLE _halide_dir
                OUTPUT_STRIP_TRAILING_WHITESPACE
                ERROR_QUIET
                RESULT_VARIABLE _halide_query
            )
            if(_halide_query EQUAL 0 AND EXISTS "${_halide_dir}")
                list(APPEND CMAKE_PREFIX_PATH "${_halide_dir}")
            endif()
        endif()
    endforeach()

    find_package(Halide 22.1 CONFIG QUIET)

    if(NOT Halide_FOUND)
        message(FATAL_ERROR
            "Halide 22.1+ not found.\n"
            "Install the wheel into the project virtualenv "
            "(python -m venv .venv && .venv/bin/pip install halide==22.1.0), "
            "or set HALIDE_PREFIX to an existing install.\n"
            "See the installation guide:\n"
            "https://capture-moment.github.io/docs/")
    endif()

    message(STATUS "Halide found: version ${Halide_VERSION}")

    set(Halide_FOUND TRUE PARENT_SCOPE)
    set(Halide_VERSION ${Halide_VERSION} PARENT_SCOPE)

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
    message(STATUS "║ Halide : 22.1.0")
    message(STATUS "║ Exiv2 : 0.28.7")
    message(STATUS "║ magic_enum : 0.9.7")
    message(STATUS "║ GTest : 1.18.0")
    message(STATUS "╚════════════════════════════════════════════════════════════╝")
    message(STATUS "")
endfunction()