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

# Shared shader assets, produced by the top-level build.
set(HOTARU_TEST_SHADER_DIR "${HOTARU_SHADER_OUTPUT_DIR}")

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

if(TARGET hotaru_shaders)
    add_dependencies(vulkan_pipeline_test hotaru_shaders)
    target_compile_definitions(vulkan_pipeline_test PRIVATE
        HOTARU_TEST_SHADER_DIR="${HOTARU_TEST_SHADER_DIR}"
    )
endif()

# Windowed triangle smoke test.
hotaru_add_test(triangle_test triangle_test.cpp)
target_link_libraries(triangle_test PRIVATE glfw)
if(TARGET hotaru_shaders)
    add_dependencies(triangle_test hotaru_shaders)
endif()

# Headless triangle rendering test.
hotaru_add_test(triangle_offscreen_test triangle_offscreen_test.cpp)
target_link_libraries(triangle_offscreen_test PRIVATE volk::volk)
if(TARGET hotaru_shaders)
    add_dependencies(triangle_offscreen_test hotaru_shaders)
    target_compile_definitions(triangle_offscreen_test PRIVATE
        HOTARU_TEST_SHADER_DIR="${HOTARU_SHADER_OUTPUT_DIR}"
    )
endif()
