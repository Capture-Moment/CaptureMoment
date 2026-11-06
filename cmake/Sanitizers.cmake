# cmake/Sanitizers.cmake

message(STATUS "Sanitizers enabled")

if(MSVC)
    # MSVC: ASan only
    add_compile_options(/fsanitize=address /Zi)
    add_compile_definitions(_DISABLE_STRING_ANNOTATION _DISABLE_VECTOR_ANNOTATION _DISABLE_OPTIONAL_ANNOTATION)
    add_link_options(/INCREMENTAL:NO /DEBUG)
else()
    set(SAN_FLAGS
        -fsanitize=address,undefined,float-divide-by-zero,float-cast-overflow
        -fno-sanitize-recover=all
        -fno-omit-frame-pointer
    )
    add_compile_options(${SAN_FLAGS} -D_GLIBCXX_ASSERTIONS)
    add_link_options(-fsanitize=address,undefined,float-divide-by-zero,float-cast-overflow)
endif()
