cmake_minimum_required(VERSION 3.20)

project(rasta VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_GENERATOR MATCHES "Makefiles")
    message(WARNING
        "This project prefers Unix Makefiles. Configure with "
        "-G \"Unix Makefiles\" when possible.")
endif()

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

enable_testing()

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED sdl2)

add_library(rasta_warnings INTERFACE)
target_compile_options(rasta_warnings INTERFACE
    -Wall
    -Wextra
    -Wpedantic
)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(rasta_warnings INTERFACE
        $<$<CONFIG:Debug>:-g>
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
    )
    target_link_options(rasta_warnings INTERFACE
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
    )
endif()

add_subdirectory(src)
add_subdirectory(samples)
add_subdirectory(tests)
