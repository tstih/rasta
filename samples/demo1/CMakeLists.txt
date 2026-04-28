add_executable(demo1
    demo1.cpp
)

target_link_libraries(demo1
    PRIVATE
        rasta_core
)

set_target_properties(demo1 PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin
)
