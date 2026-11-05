# CompilerDetection.cmake - Optimized compiler detection

# ============================================================
# Compiler requirements (C++23)
# ============================================================
# Format: COMPILER_ID|MIN_VERSION|NAME
set(SUPPORTED_COMPILERS
    "MSVC|19.44|MSVC (Visual Studio 2026 18.0+)"
    "GNU|16.0|GCC"
    "Clang|16.0|Clang"
    "AppleClang|15.0|Apple Clang (Xcode 15+)"
    "Intel|2023.0|Intel oneAPI"
    "IntelLLVM|2023.0|Intel LLVM"
    "ARMClang|16.0|ARM Clang"
)

function(verify_cpp23_support)
    foreach(ENTRY ${SUPPORTED_COMPILERS})
        string(REPLACE "|" ";" ENTRY_LIST "${ENTRY}")

        list(GET ENTRY_LIST 0 COMPILER_ID)
        list(GET ENTRY_LIST 1 MIN_VERSION)
        list(GET ENTRY_LIST 2 NAME)

        if(CMAKE_CXX_COMPILER_ID STREQUAL "${COMPILER_ID}")

            if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS MIN_VERSION)
                message(FATAL_ERROR
                    "${NAME} ${CMAKE_CXX_COMPILER_VERSION} is too old: "
                    "version ${MIN_VERSION}+ is required for C++23. "
                    "Please upgrade your compiler."
                    "\nCompiler used: ${CMAKE_CXX_COMPILER}")
            endif()

            message(STATUS "C++23 support verified (${NAME} ${CMAKE_CXX_COMPILER_VERSION})")
            return()
        endif()
    endforeach()

    message(WARNING "Compiler '${CMAKE_CXX_COMPILER_ID}' not officially tested. Build may fail.")
endfunction()

function(get_compiler_name OUTPUT_VAR)
    set(RESULT "${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
    
    foreach(ENTRY ${SUPPORTED_COMPILERS})
        string(REPLACE "|" ";" ENTRY_LIST "${ENTRY}")
        
        list(GET ENTRY_LIST 0 COMPILER_ID)
        list(GET ENTRY_LIST 2 NAME)
        
        if(CMAKE_CXX_COMPILER_ID STREQUAL "${COMPILER_ID}")
            set(RESULT "${NAME} ${CMAKE_CXX_COMPILER_VERSION}")
            break()
        endif()
    endforeach()
    
    set(${OUTPUT_VAR} "${RESULT}" PARENT_SCOPE)
endfunction()

# ============================================================
# Apply compiler-specific flags to a target
# ============================================================
function(apply_compiler_flags TARGET_NAME)

    # Skip non-compilable targets (INTERFACE_LIBRARY, generated targets, etc.)
    get_target_property(TARGET_TYPE ${TARGET_NAME} TYPE)
    
    if(TARGET_TYPE STREQUAL "INTERFACE_LIBRARY")
        return()
    endif()
    
    # Skip LLVM-generated targets
    if(TARGET_NAME MATCHES "^(intrinsics_gen|omp_gen|acc_gen|.*TableGen)$")
        return()
    endif()
    
    if(MSVC)
        target_compile_options(${TARGET_NAME} PRIVATE
            /W4                    # Warning level 4
            /std:c++latest         # C++23
            /permissive-           # Standards conformance
            /Zc:inline             # Remove unreferenced COMDAT
        )
    else()
        target_compile_options(${TARGET_NAME} PRIVATE
            -Wall                  # All warnings
            -Wextra                # Extra warnings
            -pedantic              # Pedantic warnings
            -fPIC                  # Position independent code
        )
    endif()
endfunction()


# ============================================================
# Apply compiler flags to all targets in a directory
# ============================================================
function(apply_compiler_flags_recursive DIRECTORY)
    # Get all targets in the specified directory
    get_property(TARGETS DIRECTORY ${DIRECTORY} PROPERTY BUILDSYSTEM_TARGETS)
    
    foreach(TARGET ${TARGETS})
        apply_compiler_flags(${TARGET})
        message(STATUS "[CompilerDetection] Applied flags to target: ${TARGET}")
    endforeach()
endfunction()