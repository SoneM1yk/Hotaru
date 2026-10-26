find_package(GTest REQUIRED)
find_package(glfw3 REQUIRED)

include(GoogleTest)

add_executable(backend_result_test backend_result_test.cpp)
target_link_libraries(backend_result_test PRIVATE ${PROJECT_NAME} GTest::gtest_main)
gtest_discover_tests(backend_result_test)

add_executable(vulkan_init_test vulkan_init_test.cpp)
target_link_libraries(vulkan_init_test PRIVATE ${PROJECT_NAME} GTest::gtest_main glfw)
gtest_discover_tests(vulkan_init_test)
