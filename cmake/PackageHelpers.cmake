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
    find_halide_autoschedulers()

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
# ============================================================
function(find_halide_package)

    message(STATUS "Searching for Halide...")

    set(_halide_version "22.1.0")
    set(_venv_dir "${CMAKE_SOURCE_DIR}/.venv")

    if(WIN32)
        set(_venv_python "${_venv_dir}/Scripts/python.exe")
    else()
        set(_venv_python "${_venv_dir}/bin/python")
    endif()

    # 1. Create the venv if it does not exist
    if(NOT EXISTS "${_venv_python}")
        find_package(Python COMPONENTS Interpreter REQUIRED)
        message(STATUS "Halide: creating project virtualenv in ${_venv_dir}...")
        execute_process(
            COMMAND "${Python_EXECUTABLE}" -m venv "${_venv_dir}"
            RESULT_VARIABLE _venv_rc
        )
        if(NOT _venv_rc EQUAL 0 OR NOT EXISTS "${_venv_python}")
            message(FATAL_ERROR "Failed to create the virtualenv in ${_venv_dir}.")
        endif()
    endif()

    # 2. Check whether halide is already installed in the venv
    execute_process(
        COMMAND "${_venv_python}" -c "import halide; print(halide.install_dir())"
        OUTPUT_VARIABLE _halide_dir
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        RESULT_VARIABLE _halide_query
    )

    # 3. Install halide into the venv if missing (or wrong version)
    if(NOT _halide_query EQUAL 0)
        message(STATUS "Halide: installing halide==${_halide_version} into .venv...")
        execute_process(
            COMMAND "${_venv_python}" -m pip install "halide==${_halide_version}"
            RESULT_VARIABLE _pip_rc
        )
        if(NOT _pip_rc EQUAL 0)
            message(FATAL_ERROR "Failed to install halide==${_halide_version} into .venv.")
        endif()

        execute_process(
            COMMAND "${_venv_python}" -c "import halide; print(halide.install_dir())"
            OUTPUT_VARIABLE _halide_dir
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
            RESULT_VARIABLE _halide_query
        )
        if(NOT _halide_query EQUAL 0)
            message(FATAL_ERROR "halide installed but 'import halide' still fails in .venv.")
        endif()
    endif()

    file(TO_CMAKE_PATH "${_halide_dir}" _halide_dir)
    message(STATUS "Halide install dir: ${_halide_dir}")

    # 4. Locate the CMake package inside the venv
    find_package(Halide ${_halide_version} CONFIG REQUIRED
        PATHS 
          "${_halide_dir}"
          "${_venv_dir}"
        NO_DEFAULT_PATH
    )

    if(NOT TARGET Halide::Halide)
        message(FATAL_ERROR "Halide found, but target Halide::Halide is missing.")
    endif()

    set(Halide_FOUND TRUE PARENT_SCOPE)
    set(Halide_VERSION "${Halide_VERSION}" PARENT_SCOPE)
    set(HALIDE_PYTHON_EXECUTABLE "${_venv_python}" PARENT_SCOPE)

endfunction()

# ============================================================
# Halide autoscheduler runtime plugins
#
# Called after find_halide_package().
# ============================================================
function(find_halide_autoschedulers)

    if(TARGET Halide::Adams2019 AND TARGET Halide::Anderson2021)
        return()
    endif()

    if(NOT HALIDE_PYTHON_EXECUTABLE)
        message(FATAL_ERROR "HALIDE_PYTHON_EXECUTABLE is not set: call find_halide_package() first.")
    endif()

    # <venv>/bin/python or <venv>/Scripts/python.exe  ->  <venv>
    get_filename_component(_venv_root "${HALIDE_PYTHON_EXECUTABLE}/../.." ABSOLUTE)

    execute_process(
        COMMAND "${HALIDE_PYTHON_EXECUTABLE}" -c "import halide; print(halide.install_dir())"
        OUTPUT_VARIABLE _halide_dir
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        RESULT_VARIABLE _rc)
    file(TO_CMAKE_PATH "${_halide_dir}" _halide_dir)

    message(STATUS "Halide autoschedulers: Halide_DIR=${Halide_DIR}")
    message(STATUS "Halide autoschedulers: WITH_AUTOSCHEDULERS=${WITH_AUTOSCHEDULERS}")

    # HalideAutoschedulers is a separate CMake package. Search next to the
    # Halide package config, in the halide install dir, and in the venv root.
    find_package(HalideAutoschedulers CONFIG QUIET
        PATHS
            "${_halide_dir}"
            "${_venv_root}"
        NO_DEFAULT_PATH)

    if(NOT TARGET Halide::Adams2019 OR NOT TARGET Halide::Anderson2021)
        message(FATAL_ERROR
            "HalideAutoschedulers package not found (Halide_DIR=${Halide_DIR}, "
            "venv=${_venv_root}). List what the wheel installed with: "
            "find \"${_venv_root}\" -iname '*autoschedul*'")
    endif()

    message(STATUS "Halide autoschedulers: FOUND (Halide::Adams2019, Halide::Anderson2021).")
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
    
    message(STATUS "==============================================================")
    message(STATUS "|    Summary Package Information                             |")
    message(STATUS "==============================================================")
    if(spdlog_FOUND)
        message(STATUS "|   spdlog : ${spdlog_VERSION}")
    else()
        message(STATUS "|   spdlog : Not Found")
    endif()

    if(OpenImageIO_FOUND)
        message(STATUS "|   OpenImageIO : ${OpenImageIO_VERSION}")
    else()
        message(STATUS "|   OpenImageIO : Not Found")
    endif()

    if(Halide_FOUND)
        message(STATUS "|   Halide : ${Halide_VERSION}")
    else()
        message(STATUS "|   Halide : Not Found")
    endif()

    if(Exiv2_FOUND)
        message(STATUS "|   Exiv2 : ${exiv2_VERSION}")
    else()
        message(STATUS "|   Exiv2 : Not Found")
    endif()

    if(magic_enum_FOUND)
        message(STATUS "|   magic_enum : ${magic_enum_VERSION}")
    else()
        message(STATUS "|   magic_enum : Not Found")
    endif()
    if(GTest_FOUND)
        message(STATUS "|   GTest : ${GTest_VERSION}")
    else()
        message(STATUS "|   GTest : Not Found or not needed (tests disabled)")
    endif()

    message(STATUS "==============================================================")
    message(STATUS "")

    message(STATUS "==============================================================")
    message(STATUS "|    Preferred Version                                       |")
    message(STATUS "==============================================================")
    message(STATUS "|   spdlog : 1.17.0")
    message(STATUS "|   OpenImageIO : 3.1.14.0")
    message(STATUS "|   Halide : 22.1.0")
    message(STATUS "|   Exiv2 : 0.28.7")
    message(STATUS "|   magic_enum : 0.9.7")
    message(STATUS "|   GTest : 1.18.0")
    message(STATUS "==============================================================")
    message(STATUS "")
endfunction()