include(../cmake/create_example.cmake)

# List sources
set(SOURCES
    src/main.cpp
    src/uniforms.h
    src/primitives.h
    src/draw_calls.h
    src/draw_calls.cpp
    src/runtime.h
    src/runtime.cpp
    src/csm.h
    src/csm.cpp
    src/render_context.h
    src/render_context.cpp
    )
source_group("src" FILES ${SOURCES})

# Setup example
create_example(01_basic ${SOURCES})

if(BUILD_TESTING AND DEVKIT_EXAMPLES_BUILD_DEVKIT_TESTS)
    find_package(GTest CONFIG REQUIRED)
    add_executable(01_basic_csm_tests tests/csm_test.cpp src/csm.cpp)
    target_link_libraries(01_basic_csm_tests PRIVATE devkit::gfx GTest::gtest_main)
    include(GoogleTest)
    gtest_discover_tests(01_basic_csm_tests)

    create_example(01_basic_render_pass_tests
        tests/render_pass_test.cpp
        src/runtime.cpp src/draw_calls.cpp src/render_context.cpp src/csm.cpp)
    target_link_libraries(01_basic_render_pass_tests PRIVATE GTest::gtest_main)
    gtest_discover_tests(01_basic_render_pass_tests)
endif()
