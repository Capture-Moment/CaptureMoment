# cmake/Diagnostics.cmake
#
# User-driven local diagnostics configuration:
#   - Windows : AddressSanitizer (ASan)
#   - macOS   : AddressSanitizer + UBSan + TSan
#   - Linux   : Valgrind memcheck (runtime via ctest -T memcheck)

message(STATUS "Configuring diagnostics...")

if(MSVC)
    # --------------------------------------------------------
    # Windows (MSVC): AddressSanitizer only
    # --------------------------------------------------------
    message(STATUS "-> Windows: Enabling AddressSanitizer (ASan)")
    add_compile_options(/fsanitize=address /Zi)
    add_compile_definitions(_DISABLE_STRING_ANNOTATION _DISABLE_VECTOR_ANNOTATION _DISABLE_OPTIONAL_ANNOTATION)
    add_link_options(/INCREMENTAL:NO /DEBUG)

elseif(APPLE)
    # --------------------------------------------------------
    # macOS (Clang): ASan + UBSan
    # --------------------------------------------------------
    message(STATUS "-> macOS: Enabling ASan + UBSan")
    set(SAN_FLAGS
        -fsanitize=address,undefined,float-divide-by-zero,float-cast-overflow
        -fno-sanitize-recover=all
        -fno-omit-frame-pointer
    )
    add_compile_options(${SAN_FLAGS})
    add_link_options(${SAN_FLAGS})

else()
    # --------------------------------------------------------
    # Linux: No compile-time sanitizers, rely on Valgrind
    # --------------------------------------------------------
    message(STATUS "-> Linux: Skipping compile-time sanitizers (relying on Valgrind)")
endif()

# ============================================================
# Valgrind (Linux runtime memory checker)
# ============================================================
if(UNIX AND NOT APPLE)
    find_program(VALGRIND_EXECUTABLE valgrind)
    if(VALGRIND_EXECUTABLE)
        set(MEMORYCHECK_COMMAND ${VALGRIND_EXECUTABLE})
        set(MEMORYCHECK_COMMAND_OPTIONS
            "--leak-check=full --show-leak-kinds=definite,indirect --track-origins=yes --error-exitcode=1"
            CACHE STRING "Valgrind options" FORCE)
        message(STATUS "   -> Valgrind found")
    endif()
endif()
