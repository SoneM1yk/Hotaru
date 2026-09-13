find_package(GTest REQUIRED)

# Triangle Test
add_executable(triangle_test triangle_test.cpp)
target_link_libraries(triangle_test PRIVATE ${PROJECT_NAME} GTest::gtest_main)
add_test(NAME TriangleTest COMMAND triangle_test)
