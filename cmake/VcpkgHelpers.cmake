# VcpkgHelpers.cmake - vcpkg configuration helpers for CMake projects

# Options vcpkg
option(AUTO_DETECT_TOOLCHAIN "Auto-detect compiler for vcpkg triplet" OFF)

# ============================================================
# Configure vcpkg if present
# ============================================================
function(configure_vcpkg_if_present)

    # Set default binary cache if not defined
    if(NOT DEFINED VCPKG_BINARY_SOURCES)
    set(VCPKG_BINARY_SOURCES "clear;files,${VCPKG_ROOT}/download_cache" CACHE STRING "Vcpkg binary sources")
    message(STATUS "Binary cache for Vcpkg enabled and configured locally.")
    endif()

    # vevify vcpkg toolchain file is used
    if(NOT DEFINED CMAKE_TOOLCHAIN_FILE OR NOT "${CMAKE_TOOLCHAIN_FILE}" MATCHES "vcpkg.cmake$")
        return() # not using vcpkg
    endif()

    message(STATUS "vcpkg detected: ${CMAKE_TOOLCHAIN_FILE}")

    # Auto-detection of triplet if requested
    if(AUTO_DETECT_TOOLCHAIN)
        detect_and_set_vcpkg_triplet()
    else()
        ensure_vcpkg_triplet_is_set()
    endif()

    # Consistency validation
    validate_vcpkg_triplet()

    # Binary cache configuration
    configure_vcpkg_binary_cache()

    # Display vcpkg configuration
    print_vcpkg_configuration()
endfunction()

# ============================================================
# Automatic detection of vcpkg triplet
# ============================================================
function(detect_and_set_vcpkg_triplet)
    if(DEFINED VCPKG_TARGET_TRIPLET)
        return() # Already defined by the user
    endif()

    # Determination of triplet based on compiler and build type
    if(DETECTED_COMPILER STREQUAL "MSVC")
        if(CMAKE_BUILD_TYPE STREQUAL "Debug")
            set(VCPKG_TARGET_TRIPLET "x64-windows" CACHE STRING "vcpkg triplet (auto-detected)")
            message(STATUS "Triplet auto-selected: x64-windows (MSVC Debug)")
        else()
            set(VCPKG_TARGET_TRIPLET "x64-windows-release" CACHE STRING "vcpkg triplet (auto-detected)")
            message(STATUS "Triplet auto-selected: x64-windows-release (MSVC Release)")
        endif()
    elseif(DETECTED_COMPILER STREQUAL "MinGW")
        if(CMAKE_BUILD_TYPE STREQUAL "Debug")
            set(VCPKG_TARGET_TRIPLET "x64-mingw-dynamic" CACHE STRING "vcpkg triplet (auto-detected)")
            message(STATUS "Triplet auto-selected: x64-mingw-dynamic (MinGW Debug)")
        else()
            set(VCPKG_TARGET_TRIPLET "x64-mingw-dynamic-release" CACHE STRING "vcpkg triplet (auto-detected)")
            message(STATUS "Triplet auto-selected: x64-mingw-dynamic-release (MinGW Release)")
        endif()
    elseif(DETECTED_COMPILER STREQUAL "Clang")
        set(VCPKG_TARGET_TRIPLET "x64-windows-llvm" CACHE STRING "vcpkg triplet (auto-detected)")
        message(STATUS "Triplet auto-selected: x64-windows-llvm (Clang)")
    else()
        message(WARNING "Compiler not supported for auto-detection, using x64-windows by default")
        set(VCPKG_TARGET_TRIPLET "x64-windows" CACHE STRING "vcpkg triplet (default)")
    endif()
    
    message(STATUS "✅ Triplet auto-selected: ${VCPKG_TARGET_TRIPLET}")
endfunction()

# ============================================================
# Ensure vcpkg triplet is set
# ============================================================
function(ensure_vcpkg_triplet_is_set)
    if(NOT DEFINED VCPKG_TARGET_TRIPLET)
        # Default value if not specified
        set(VCPKG_TARGET_TRIPLET "x64-windows" CACHE STRING "vcpkg triplet (forced default)")
        message(STATUS "VCPKG_TARGET_TRIPLET not defined, using '${VCPKG_TARGET_TRIPLET}' by default")
    else()
        message(STATUS "Using specified VCPKG_TARGET_TRIPLET: '${VCPKG_TARGET_TRIPLET}'")
    endif()
endfunction()

# ============================================================
# Validate vcpkg triplet consistency
# ============================================================
function(validate_vcpkg_triplet)

    if(DETECTED_COMPILER STREQUAL "MSVC" AND NOT VCPKG_TARGET_TRIPLET MATCHES "x64-windows")
        message(WARNING "Inconsistency: MSVC compiler but triplet '${VCPKG_TARGET_TRIPLET}'. Build issues may occur.")
    elseif(DETECTED_COMPILER STREQUAL "MinGW" AND NOT VCPKG_TARGET_TRIPLET MATCHES "mingw")
        message(WARNING "Inconsistency: MinGW compiler but triplet '${VCPKG_TARGET_TRIPLET}'. Build issues may occur.")
    endif()
endfunction()

# ============================================================
# Configure vcpkg binary cache
# ============================================================
function(configure_vcpkg_binary_cache)
    # vcpkg binary cache is self -configurable.
    # the user can set it via:
    # - Environment variable : VCPKG_BINARY_SOURCES
    # - vcpkg-configuration.json file
    # - CLI option
    if(DEFINED ENV{VCPKG_BINARY_SOURCES})
        message(STATUS "vcpkg binary cache configured via environment variable")
    elseif(EXISTS "${CMAKE_SOURCE_DIR}/vcpkg-configuration.json")
        message(STATUS "vcpkg binary cache configured via vcpkg-configuration.json")
    else()
        message(STATUS "Tip: Enable vcpkg binary cache to speed up builds:")
        message(STATUS "export VCPKG_BINARY_SOURCES=\"clear;default,readwrite\"")
        message(STATUS "Or use: --binarysource=clear;default,readwrite")
    endif()
endfunction()

# ============================================================
# Print vcpkg configuration
# ============================================================
function(print_vcpkg_configuration)

    message(STATUS "")
    message(STATUS "==============================================================")
    message(STATUS "|  vcpkg Configuration                                       |")
    message(STATUS "==============================================================")
    message(STATUS "|  Compiler        : ${DETECTED_COMPILER} (${CMAKE_CXX_COMPILER_ID})")
    message(STATUS "|  Build Type      : ${CMAKE_BUILD_TYPE}")
    message(STATUS "|  Triplet         : ${VCPKG_TARGET_TRIPLET}")
    message(STATUS "==============================================================")
    message(STATUS "")
endfunction()