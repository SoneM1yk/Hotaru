cmake_minimum_required(VERSION 3.21)

project(hotaru LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_TEST ON)
option(ENABLE_ASAN OFF)
option(ENABLE_PROFILE ON)

if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.12")
    cmake_policy(SET CMP0074 NEW)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Shader compilation shared by the engine and tests.
find_program(HOTARU_GLSLC_EXECUTABLE
    NAMES glslc
    HINTS
        "$ENV{VULKAN_SDK}/bin"
        "$ENV{HOME}/Lib/vulkan/bin"
)
find_program(HOTARU_GLSLANG_VALIDATOR_EXECUTABLE
    NAMES glslangValidator
    HINTS
        "$ENV{VULKAN_SDK}/bin"
        "$ENV{HOME}/Lib/vulkan/bin"
)

set(HOTARU_SHADER_OUTPUT_DIR "${CMAKE_BINARY_DIR}/shader")
set(HOTARU_VERTEX_SHADER "${HOTARU_SHADER_OUTPUT_DIR}/default.vert.spv")
set(HOTARU_FRAGMENT_SHADER "${HOTARU_SHADER_OUTPUT_DIR}/default.frag.spv")

if(HOTARU_GLSLC_EXECUTABLE)
    set(HOTARU_VERTEX_SHADER_COMMAND
        "${HOTARU_GLSLC_EXECUTABLE}" -fshader-stage=vert
        "${CMAKE_SOURCE_DIR}/shader/default.vert" -o "${HOTARU_VERTEX_SHADER}")
    set(HOTARU_FRAGMENT_SHADER_COMMAND
        "${HOTARU_GLSLC_EXECUTABLE}" -fshader-stage=frag
        "${CMAKE_SOURCE_DIR}/shader/default.frag" -o "${HOTARU_FRAGMENT_SHADER}")
elseif(HOTARU_GLSLANG_VALIDATOR_EXECUTABLE)
    set(HOTARU_VERTEX_SHADER_COMMAND
        "${HOTARU_GLSLANG_VALIDATOR_EXECUTABLE}" -V
        "${CMAKE_SOURCE_DIR}/shader/default.vert" -o "${HOTARU_VERTEX_SHADER}")
    set(HOTARU_FRAGMENT_SHADER_COMMAND
        "${HOTARU_GLSLANG_VALIDATOR_EXECUTABLE}" -V
        "${CMAKE_SOURCE_DIR}/shader/default.frag" -o "${HOTARU_FRAGMENT_SHADER}")
endif()

if(HOTARU_VERTEX_SHADER_COMMAND)
    add_custom_command(
        OUTPUT ${HOTARU_VERTEX_SHADER} ${HOTARU_FRAGMENT_SHADER}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${HOTARU_SHADER_OUTPUT_DIR}
        COMMAND ${HOTARU_VERTEX_SHADER_COMMAND}
        COMMAND ${HOTARU_FRAGMENT_SHADER_COMMAND}
        DEPENDS
            ${CMAKE_SOURCE_DIR}/shader/default.vert
            ${CMAKE_SOURCE_DIR}/shader/default.frag
        COMMENT "Compiling Hotaru GLSL shaders to SPIR-V"
        VERBATIM
    )
    add_custom_target(hotaru_shaders
        DEPENDS ${HOTARU_VERTEX_SHADER} ${HOTARU_FRAGMENT_SHADER})
else()
    message(WARNING
        "glslc/glslangValidator not found; the triangle demo will have no "
        "SPIR-V shaders. Install glslang-tools or the Vulkan SDK.")
endif()

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
