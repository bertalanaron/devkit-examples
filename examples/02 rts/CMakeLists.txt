include(../cmake/create_example.cmake)

# List sources
set(SOURCES
    src/main.cpp)
source_group("src" FILES ${SOURCES})

# Setup example
create_example(02_rts ${SOURCES})
