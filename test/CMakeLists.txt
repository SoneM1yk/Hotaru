find_package(GTest REQUIRED)
find_package(glfw3 REQUIRED)
find_package(volk REQUIRED)

include(GoogleTest)

function(hotaru_add_test target)
    add_executable(${target} ${ARGN})
    target_link_libraries(${target} PRIVATE
        ${PROJECT_NAME}
        GTest::gtest_main
    )
    target_include_directories(${target} PRIVATE ${CMAKE_SOURCE_DIR}/src)
    gtest_discover_tests(${target})
endfunction()

# Shared shader assets for Vulkan pipeline tests.
find_program(GLSLC_EXECUTABLE
    NAMES glslc
    HINTS
        "$ENV{VULKAN_SDK}/bin"
        "$ENV{HOME}/Lib/vulkan/bin"
)

if(GLSLC_EXECUTABLE)
    set(HOTARU_TEST_SHADER_DIR "${CMAKE_CURRENT_BINARY_DIR}/shader")
    set(HOTARU_TEST_VERTEX_SHADER
        "${HOTARU_TEST_SHADER_DIR}/default.vert.spv")
    set(HOTARU_TEST_FRAGMENT_SHADER
        "${HOTARU_TEST_SHADER_DIR}/default.frag.spv")

    add_custom_command(
        OUTPUT ${HOTARU_TEST_VERTEX_SHADER} ${HOTARU_TEST_FRAGMENT_SHADER}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${HOTARU_TEST_SHADER_DIR}
        COMMAND ${GLSLC_EXECUTABLE}
            -o ${HOTARU_TEST_VERTEX_SHADER}
            ${CMAKE_SOURCE_DIR}/shader/default.vert
        COMMAND ${GLSLC_EXECUTABLE}
            -o ${HOTARU_TEST_FRAGMENT_SHADER}
            ${CMAKE_SOURCE_DIR}/shader/default.frag
        DEPENDS
            ${CMAKE_SOURCE_DIR}/shader/default.vert
            ${CMAKE_SOURCE_DIR}/shader/default.frag
        COMMENT "Compiling GLSL shaders for Vulkan tests"
        VERBATIM
    )

    add_custom_target(hotaru_test_shaders
        DEPENDS ${HOTARU_TEST_VERTEX_SHADER} ${HOTARU_TEST_FRAGMENT_SHADER}
    )
endif()

# Backend result tests.
hotaru_add_test(backend_result_test backend_result_test.cpp)

# Vulkan initialization and swapchain tests.
hotaru_add_test(vulkan_init_test vulkan_init_test.cpp)
target_link_libraries(vulkan_init_test PRIVATE
    glfw
    volk::volk
)

# Vulkan pipeline tests.
hotaru_add_test(vulkan_pipeline_test vulkan_pipeline_test.cpp)
target_link_libraries(vulkan_pipeline_test PRIVATE volk::volk)

if(GLSLC_EXECUTABLE)
    add_dependencies(vulkan_pipeline_test hotaru_test_shaders)
    target_compile_definitions(vulkan_pipeline_test PRIVATE
        HOTARU_TEST_SHADER_DIR="${HOTARU_TEST_SHADER_DIR}"
    )
endif()
