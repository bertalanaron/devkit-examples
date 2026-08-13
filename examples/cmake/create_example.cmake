function(create_example example_name)
    # Set sources
    set(common_sources
        ${PROJECT_SOURCE_DIR}/examples/common/src/example_application.h)
    source_group("common" FILES ${common_sources})
    # Get sources from arguments and join with common sources
    set(example_sources ${common_sources} ${ARGN})
    
    # Create executable
	add_executable(${example_name} ${example_sources})
    target_include_directories(${example_name} PRIVATE ${PROJECT_SOURCE_DIR})
    target_link_libraries(${example_name} PRIVATE devkit::devkit)
    set_target_properties(${example_name} PROPERTIES FOLDER "examples")

    # Configure ini
    get_filename_component(DATA_PATH "${CMAKE_CURRENT_SOURCE_DIR}/data" ABSOLUTE)
    configure_file(
        ${PROJECT_SOURCE_DIR}/examples/examples.ini.in
        ${CMAKE_BINARY_DIR}/${example_name}.ini
        @ONLY
    )
    
    # Move ini next to exe at build
    add_custom_command(TARGET ${example_name} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_BINARY_DIR}/${example_name}.ini"
            "$<TARGET_FILE_DIR:${example_name}>/${example_name}.ini"
    )
endfunction()
