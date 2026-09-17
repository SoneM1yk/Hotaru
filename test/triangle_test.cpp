#include <GLFW/glfw3.h>
#include <gtest/gtest.h>
#include <hotaru/engine.h>

TEST(TriangleTest, Display) 
{
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    GLFWwindow* window = glfwCreateWindow(800, 600, "Test", nullptr, nullptr);

    Hotaru::Engine engine;
    engine.Init();

    

    SUCCEED();
}