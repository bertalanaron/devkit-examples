option(DEVKIT_EXAMPLES_BUILD_00_SELECT "Build the 00_select example" OFF)
option(DEVKIT_EXAMPLES_BUILD_01_BASIC "Build the 01_basic example" ON)
option(DEVKIT_EXAMPLES_BUILD_02_RTS "Build the 02_rts example" OFF)
option(DEVKIT_EXAMPLES_BUILD_03_TERRAIN "Build the 03_terrain example" OFF)
option(DEVKIT_EXAMPLES_BUILD_04_NEWTERRAIN "Build the 04_newterrain example" ON)
option(DEVKIT_EXAMPLES_BUILD_ALL "Build all examples" OFF)

if(DEVKIT_EXAMPLES_BUILD_ALL OR DEVKIT_EXAMPLES_BUILD_00_SELECT)
    add_subdirectory("00 select")
endif()

if(DEVKIT_EXAMPLES_BUILD_ALL OR DEVKIT_EXAMPLES_BUILD_01_BASIC)
    add_subdirectory("01 basic")
endif()

if(DEVKIT_EXAMPLES_BUILD_ALL OR DEVKIT_EXAMPLES_BUILD_02_RTS)
    add_subdirectory("02 rts")
endif()

if(DEVKIT_EXAMPLES_BUILD_ALL OR DEVKIT_EXAMPLES_BUILD_03_TERRAIN)
    add_subdirectory("03 terrain")
endif()

if(DEVKIT_EXAMPLES_BUILD_ALL OR DEVKIT_EXAMPLES_BUILD_04_NEWTERRAIN)
    add_subdirectory("04 new terrain")
endif()
