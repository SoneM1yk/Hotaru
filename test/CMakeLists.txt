find_package(GTest REQUIRED)
find_package(glfw3 REQUIRED)
find_package(volk REQUIRED)

include(GoogleTest)

function(hotaru_add_test target)
    add_executable(${target} ${ARGN})
    target_link_libraries(${target} PRIVATE
        ${PROJECT_NAME}
        GTest::gtest_main
        glfw
        volk::volk
    )
    target_include_directories(${target} PRIVATE ${CMAKE_SOURCE_DIR}/src)
    gtest_discover_tests(${target} PROPERTIES TIMEOUT 60)
endfunction()

# Backend result tests.
hotaru_add_test(backend_result_test backend_result_test.cpp)

# Vulkan initialization and swapchain tests.
hotaru_add_test(vulkan_init_test vulkan_init_test.cpp)

# Vulkan pipeline tests.
hotaru_add_test(vulkan_pipeline_test vulkan_pipeline_test.cpp)

# Vulkan triangle tests.
hotaru_add_test(vulkan_triangle_test vulkan_triangle_test.cpp)


target_compile_definitions(vulkan_pipeline_test PRIVATE
    HOTARU_TEST_SHADER_DIR="${HOTARU_SHADER_BINARY_DIR}"
)
