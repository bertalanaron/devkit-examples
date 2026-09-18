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
    src/render_context.h
    src/render_context.cpp
    )
source_group("src" FILES ${SOURCES})

# Setup example
create_example(01_basic ${SOURCES})
