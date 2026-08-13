cmake_minimum_required(VERSION 3.21)

project(devkit-examples LANGUAGES CXX)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(devkit CONFIG REQUIRED)

add_subdirectory(examples)
