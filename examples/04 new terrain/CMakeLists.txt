include(../cmake/create_example.cmake)

set(SOURCES
    src/main.cpp
    src/terrain.h
    src/terrain.cpp)
source_group("src" FILES ${SOURCES})
	
create_example(04_newterrain ${SOURCES})
