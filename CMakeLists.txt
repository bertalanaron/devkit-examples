cmake_minimum_required(VERSION 3.21)

project(devkit-examples LANGUAGES CXX)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(DEVKIT_EXAMPLES_USE_LOCAL_DEVKIT "Build the devkit checkout in this repository instead of the vcpkg devkit package" ON)
option(DEVKIT_EXAMPLES_BUILD_DEVKIT_TESTS "Build devkit tests when using the local devkit checkout" ON)

if(DEVKIT_EXAMPLES_USE_LOCAL_DEVKIT AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/devkit/CMakeLists.txt")
    if(DEVKIT_EXAMPLES_BUILD_DEVKIT_TESTS)
        include(CTest)
        enable_testing()
        set(DEVKIT_BUILD_TESTS ON)
    endif()
    add_subdirectory(devkit)
else()
    find_package(devkit CONFIG REQUIRED)
endif()

add_subdirectory(examples)
