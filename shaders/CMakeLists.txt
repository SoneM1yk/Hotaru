find_program(GLSLC_EXECUTABLE NAMES glslc)

if(NOT GLSLC_EXECUTABLE)
    message(FATAL_ERROR
        "glslc was not found. Install the Vulkan SDK or set VULKAN_SDK.")
endif()

set(HOTARU_SHADER_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(HOTARU_EMBED_SPIRV_SCRIPT
    ${PROJECT_SOURCE_DIR}/cmake/EmbedSpirv.cmake)
set(HOTARU_SHADER_OUTPUT_DIR
    ${CMAKE_CURRENT_BINARY_DIR}/generated)
set(HOTARU_SHADER_INCLUDE_DIR
    ${HOTARU_SHADER_OUTPUT_DIR}/include)

set(HOTARU_SHADER_EXTENSIONS .vert .frag .comp)
set(HOTARU_SHADER_SOURCES)
foreach(shader_extension IN LISTS HOTARU_SHADER_EXTENSIONS)
    file(GLOB shader_files CONFIGURE_DEPENDS
        ${HOTARU_SHADER_SOURCE_DIR}/*${shader_extension})
    list(APPEND HOTARU_SHADER_SOURCES ${shader_files})
endforeach()
list(SORT HOTARU_SHADER_SOURCES)

# Re-run CMake configuration when a shader source changes, since compilation
# and header generation happen during configuration.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    ${HOTARU_SHADER_SOURCES})

# Shader compilation happens during CMake configure so the generated headers
# are available before any target starts compiling.
function(hotaru_compile_shader shader_source)
    get_filename_component(shader_name "${shader_source}" NAME)
    string(REPLACE "." "_" shader_symbol "${shader_name}")

    set(shader_spv
        "${HOTARU_SHADER_OUTPUT_DIR}/${shader_name}.spv")
    set(shader_header
        "${HOTARU_SHADER_INCLUDE_DIR}/hotaru/shaders/${shader_symbol}.h")

    execute_process(
        COMMAND "${GLSLC_EXECUTABLE}"
            --target-env=vulkan1.3
            -O
            -o "${shader_spv}"
            "${shader_source}"
        RESULT_VARIABLE shader_result
        OUTPUT_VARIABLE shader_stdout
        ERROR_VARIABLE shader_stderr
    )
    if(NOT shader_result EQUAL 0)
        message(FATAL_ERROR
            "Failed to compile ${shader_name}:\n${shader_stderr}")
    endif()

    execute_process(
        COMMAND "${CMAKE_COMMAND}"
            "-DINPUT=${shader_spv}"
            "-DOUTPUT=${shader_header}"
            "-DSYMBOL=${shader_symbol}"
            -P "${HOTARU_EMBED_SPIRV_SCRIPT}"
        RESULT_VARIABLE embed_result
        OUTPUT_VARIABLE embed_stdout
        ERROR_VARIABLE embed_stderr
    )
    if(NOT embed_result EQUAL 0)
        message(FATAL_ERROR
            "Failed to embed ${shader_name}.spv:\n${embed_stderr}")
    endif()
endfunction()

message(STATUS "Compiling Hotaru shaders")
file(MAKE_DIRECTORY
    "${HOTARU_SHADER_OUTPUT_DIR}"
    "${HOTARU_SHADER_INCLUDE_DIR}/hotaru/shaders")

foreach(shader_source IN LISTS HOTARU_SHADER_SOURCES)
    hotaru_compile_shader("${shader_source}")
endforeach()

# Header-only target used by Hotaru to include the generated shader data.
add_library(hotaru_shader_headers INTERFACE)
target_include_directories(hotaru_shader_headers INTERFACE
    ${HOTARU_SHADER_INCLUDE_DIR}
)

set(HOTARU_SHADER_BINARY_DIR
    ${HOTARU_SHADER_OUTPUT_DIR} PARENT_SCOPE)
