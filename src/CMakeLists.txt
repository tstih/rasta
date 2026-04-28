add_library(rasta_core
    framebuffer_file.cpp
    input_broadcaster.cpp
    raster_decoder.cpp
    rasta_options.cpp
    sdl_emulator.cpp
)

add_library(rasta_client_c
    rasta_client.c
)

target_include_directories(rasta_core
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
        ${SDL2_INCLUDE_DIRS}
)

target_include_directories(rasta_client_c
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(rasta_core
    PUBLIC
        rasta_warnings
        ${SDL2_LIBRARIES}
)

target_link_libraries(rasta_client_c
    PUBLIC
        rasta_warnings
)

add_executable(rasta
    main.cpp
)

target_link_libraries(rasta
    PRIVATE
        rasta_core
)

set_target_properties(rasta PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin
)
