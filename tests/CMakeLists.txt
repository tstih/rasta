add_executable(test_rasta
    test_rasta.cpp
)

target_include_directories(test_rasta
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(test_rasta
    PRIVATE
        rasta_core
        rasta_warnings
)

set_target_properties(test_rasta PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin
)

add_test(NAME test_rasta COMMAND test_rasta)
set_tests_properties(test_rasta PROPERTIES
    ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0"
)
