find_package(GTest REQUIRED)
find_package(glfw3 REQUIRED)

include(GoogleTest)

# Triangle Test
add_executable(triangle_test triangle_test.cpp)
target_link_libraries(triangle_test PRIVATE ${PROJECT_NAME} GTest::gtest_main glfw)
gtest_discover_tests(triangle_test)
