include(../cmake/create_example.cmake)

set(SOURCES
    src/main.cpp
    src/terrain.h
    src/terrain_editor.h
    src/terrain.cpp
    src/terrain_editor.cpp
    src/view.h
    src/rts_camera_controller.h
    src/editor_client.h
    src/editor_client.cpp
    src/game_client.h
    src/ui_utils.h
    src/game_state.h
    src/objects_editor.h)
source_group("src" FILES ${SOURCES})
	
create_example(03_terrain ${SOURCES})
