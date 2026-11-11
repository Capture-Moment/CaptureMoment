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
#
# Halide is distributed as a pip wheel (halide / halide-bin /
# halide-runtime). The setup scripts install it in a project-local
# virtualenv, so look there first, then fall back to an explicit
# HALIDE_PREFIX, an active virtualenv, or a system-wide install.
# ============================================================
function(find_halide_package)
    message(STATUS "Searching for Halide...")

    find_package(Halide 22.1 CONFIG QUIET)

    # Not found? Try installing the pinned wheel into the project venv.
    if(NOT Halide_FOUND)
        set(_halide_version "22.1.0")

        # Use native CMake Python3 module to find the interpreter cleanly
        find_package(Python3 COMPONENTS Interpreter REQUIRED)

        # Locate or create the project-local venv
        if(NOT EXISTS "${CMAKE_SOURCE_DIR}/.venv")
            message(STATUS "Halide: creating project virtualenv (.venv)...")
            execute_process(
                COMMAND ${Python3_EXECUTABLE} -m venv "${CMAKE_SOURCE_DIR}/.venv"
                RESULT_VARIABLE _venv_rc)
        endif()

        # Determine venv paths according to the operating system
        if(WIN32)
            set(_venv_pip "${CMAKE_SOURCE_DIR}/.venv/Scripts/pip.exe")
            set(_venv_python "${CMAKE_SOURCE_DIR}/.venv/Scripts/python.exe")
        else()
            set(_venv_pip "${CMAKE_SOURCE_DIR}/.venv/bin/pip")
            set(_venv_python "${CMAKE_SOURCE_DIR}/.venv/bin/python")
        endif()

        # Install the pinned Halide wheel into the venv
        if(EXISTS "${_venv_python}")
            message(STATUS "Halide not found — installing halide==${_halide_version} into .venv...")
            execute_process(
                COMMAND "${_venv_pip}" install "halide==${_halide_version}"
                RESULT_VARIABLE _pip_rc)
        else()
            set(_pip_rc 1)
        endif()

        if(_pip_rc EQUAL 0)
            # Query the Halide installation directory from Python
            execute_process(
                COMMAND "${_venv_python}" -c "import halide; print(halide.install_dir())"
                OUTPUT_VARIABLE _halide_dir
                OUTPUT_STRIP_TRAILING_WHITESPACE
                ERROR_QUIET
                RESULT_VARIABLE _halide_query)
            if(_halide_query EQUAL 0 AND EXISTS "${_halide_dir}")
                list(APPEND CMAKE_PREFIX_PATH "${_halide_dir}")
                find_package(Halide 22.1 CONFIG QUIET)
            endif()
        endif()
    endif()

    if(NOT Halide_FOUND)
        message(FATAL_ERROR
            "Halide 22.1+ could not be installed automatically.\n"
            "Check your network connection, or set HALIDE_PREFIX to an existing install.")
    endif()
endfunction()

# ============================================================
# Halide autoscheduler runtime plugins
#
# Called after find_halide_package().
# ============================================================
function(find_halide_autoschedulers)

    set(HALIDE_PLUGIN_FILES "" PARENT_SCOPE)

    # ------------------------------------------------------------
    # Attempt 1: official imported targets
    # ------------------------------------------------------------
    message(STATUS "Halide autoschedulers: checking for imported targets (Halide::Adams2019, Halide::Anderson2021)...")
    find_package(HalideAutoschedulers QUIET)

    if(TARGET Halide::Adams2019 AND TARGET Halide::Anderson2021)
        message(STATUS "Halide autoschedulers: FOUND via imported targets.")
        set(_resolved TRUE)
        foreach(_tgt Halide::Adams2019 Halide::Anderson2021)
            # Resolve the real file for display: generator expressions
            # like $<TARGET_FILE:...> only expand at build time, so we
            # query the imported location for the log.
            set(_loc "")
            foreach(_cfg IMPORTED_LOCATION
                         IMPORTED_LOCATION_RELEASE
                         IMPORTED_LOCATION_RELWITHDEBINFO
                         IMPORTED_LOCATION_DEBUG)
                if(NOT _loc)
                    get_target_property(_loc ${_tgt} ${_cfg})
                endif()
            endforeach()
            if(_loc AND NOT _loc STREQUAL "_loc-NOTFOUND")
                message(STATUS "   - ${_tgt} -> ${_loc}")
            else()
                message(STATUS "   - ${_tgt} -> (file resolved at build time via TARGET_FILE)")
            endif()
        endforeach()
        set(HALIDE_PLUGIN_FILES
            "$<TARGET_FILE:Halide::Adams2019>"
            "$<TARGET_FILE:Halide::Anderson2021>"
            PARENT_SCOPE)
        message(STATUS "Halide autoschedulers: deployment list OK (2 plugins).")
        return()
    endif()

    # ------------------------------------------------------------
    # Attempt 2: bounded search inside the halide install dir
    # ------------------------------------------------------------
    message(STATUS "Halide autoschedulers: imported targets not found. Falling back to halide site-packages search...")

    # The venv python is the one find_halide_package() installed the
    # wheel into (bin/ on POSIX, Scripts/ on Windows).
    find_program(_halide_python
        NAMES python python3
        HINTS
            "${CMAKE_SOURCE_DIR}/.venv/bin"
            "${CMAKE_SOURCE_DIR}/.venv/Scripts"
        NO_DEFAULT_PATH
    )
    if(NOT _halide_python)
        message(FATAL_ERROR
            "Halide autoschedulers: project venv python not found "
            "(searched .venv/bin and .venv/Scripts only, NO_DEFAULT_PATH). "
            "Cannot locate the halide site-packages directory.")
    endif()
    message(STATUS "Halide autoschedulers: using venv python: ${_halide_python}")

    # The binding reports its own install dir — the authoritative
    # location, the same call find_halide_package() already relies on.
    execute_process(
        COMMAND "${_halide_python}" -c "import halide; print(halide.install_dir())"
        OUTPUT_VARIABLE _halide_install_dir
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        RESULT_VARIABLE _halide_query_rc)
    if(NOT _halide_query_rc EQUAL 0 OR NOT EXISTS "${_halide_install_dir}")
        message(FATAL_ERROR
            "Halide autoschedulers: 'import halide' failed or returned an "
            "invalid install_dir ('${_halide_install_dir}'). "
            "Is the halide wheel installed in the project venv?")
    endif()
    message(STATUS "Halide autoschedulers: halide.install_dir() -> ${_halide_install_dir}")

    # Bounded search: autoschedule_* inside the halide install dir ONLY.
    file(GLOB_RECURSE _plugins
        "${_halide_install_dir}/autoschedule_adams2019.*"
        "${_halide_install_dir}/autoschedule_anderson2021.*"
    )

    if(NOT _plugins)
        message(FATAL_ERROR
            "Halide autoschedulers: no autoschedule_adams2019 / "
            "autoschedule_anderson2021 plugin found inside "
            "${_halide_install_dir}. Your Halide installation does not "
            "ship the autoschedulers, but Adams2019 is required at "
            "runtime by the image pipeline.")
    endif()

    list(LENGTH _plugins _n)
    if(_n LESS 2)
        message(FATAL_ERROR
            "Halide autoschedulers: expected 2 plugins "
            "(adams2019 + anderson2021), found ${_n} inside "
            "${_halide_install_dir}: ${_plugins}")
    endif()

    set(HALIDE_PLUGIN_FILES "${_plugins}" PARENT_SCOPE)
    message(STATUS "Halide autoschedulers: FOUND via halide install dir search:")
    foreach(_p IN LISTS _plugins)
        message(STATUS "   - ${_p}")
    endforeach()
    message(STATUS "Halide autoschedulers: deployment list OK (${_n} plugins).")
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
    message(STATUS "|   spdlog : 1.16.0")
    message(STATUS "|   OpenImageIO : 3.1.14.0")
    message(STATUS "|   Halide : 22.1.0")
    message(STATUS "|   Exiv2 : 0.28.7")
    message(STATUS "|   magic_enum : 0.9.7")
    message(STATUS "|   GTest : 1.18.0")
    message(STATUS "==============================================================")
    message(STATUS "")
endfunction()