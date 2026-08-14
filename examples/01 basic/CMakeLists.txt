include(../cmake/create_example.cmake)

# List sources
set(SOURCES
    src/main.cpp
    src/uniforms.h
    src/primitives.h
    src/render_context.h
    src/render_context.cpp
    )
source_group("src" FILES ${SOURCES})

# Setup example
create_example(01_basic ${SOURCES})
