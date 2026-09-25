option(DEVKIT_EXAMPLES_BUILD_SHADER_SANDBOX "Build the shader_sandbox example" ON)
option(DEVKIT_EXAMPLES_BUILD_RTS "Build the rts example" ON)
option(DEVKIT_EXAMPLES_BUILD_ALL "Build all examples" OFF)

if(DEVKIT_EXAMPLES_BUILD_ALL OR DEVKIT_EXAMPLES_BUILD_SHADER_SANDBOX)
    add_subdirectory(shader_sandbox)
endif()

if(DEVKIT_EXAMPLES_BUILD_ALL OR DEVKIT_EXAMPLES_BUILD_RTS)
    add_subdirectory(rts)
endif()
