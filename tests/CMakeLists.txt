include(GoogleTest)

file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*_test.cpp"
)

if(NOT TEST_SOURCES)
    message(WARNING "[tests] No *_test.cpp file found")
    return()
endif()

add_executable(core_tests ${TEST_SOURCES})

target_link_libraries(core_tests PRIVATE
    capturemoment_core
    GTest::gtest
    GTest::gtest_main
    GTest::gmock

    OpenImageIO::OpenImageIO
)

target_compile_features(core_tests PRIVATE cxx_std_23)

if(WIN32)
    add_custom_command(TARGET core_tests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_RUNTIME_DLLS:core_tests> $<TARGET_FILE_DIR:core_tests>
        COMMAND_EXPAND_LISTS
    )
endif()

gtest_discover_tests(core_tests
    DISCOVERY_MODE PRE_TEST
)
