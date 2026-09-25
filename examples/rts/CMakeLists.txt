add_library(rts_common STATIC
    common/include/rts/application.h
    common/src/application.cpp
)
target_include_directories(rts_common PUBLIC common/include)
target_link_libraries(rts_common PRIVATE devkit::devkit)

add_executable(rts_client client/main.cpp)
target_link_libraries(rts_client PRIVATE rts_common)

add_executable(rts_editor editor/main.cpp)
target_link_libraries(rts_editor PRIVATE rts_common)

set_target_properties(rts_common rts_client rts_editor PROPERTIES FOLDER "examples/rts")
