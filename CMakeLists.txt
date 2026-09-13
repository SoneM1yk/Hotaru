cmake_minimum_required(VERSION 3.21)

project(hotaru LANGUAGES CXX)

option(BUILD_TEST ON)
option(ENABLE_ASAN OFF)

if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.12")
    cmake_policy(SET CMP0074 NEW)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(ENABLE_ASAN)
    message(STATUS "AddressSanitizer enabled")

    add_compile_options(
        -fsanitize=address
        -fno-omit-frame-pointer
    )

    add_link_options(
        -fsanitize=address
    )
endif()

add_subdirectory(src)
if(BUILD_TEST)
    enable_testing()
    add_subdirectory(test)
endif()
