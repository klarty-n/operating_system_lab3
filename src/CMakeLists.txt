cmake_minimum_required(VERSION 3.16)

project(lab3 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

find_package(Threads REQUIRED)

add_executable(lab3
    main.c
    filter.c
)

target_compile_definitions(lab3 PRIVATE _POSIX_C_SOURCE=200809L)
target_compile_options(lab3 PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(lab3 PRIVATE Threads::Threads m)
