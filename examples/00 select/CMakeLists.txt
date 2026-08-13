# List sources
set(SOURCES
    src/main.cpp)
source_group("src" FILES ${SOURCES})

# Create executable
add_executable(00_select ${SOURCES})

target_link_libraries(00_select PRIVATE devkit)

set_target_properties(00_select PROPERTIES FOLDER "examples")


