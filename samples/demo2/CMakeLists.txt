add_executable(demo2
    demo2.c
)

target_include_directories(demo2
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(demo2
    PRIVATE
        rasta_client_c
)

set_target_properties(demo2 PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS OFF
    RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin
)
