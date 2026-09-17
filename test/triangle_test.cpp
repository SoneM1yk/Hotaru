#include "hotaru/config.h"
#include "hotaru/renderer.h"
#include <GLFW/glfw3.h>
#include <gtest/gtest.h>

#include <hotaru/entity.h>
#include <hotaru/scene.h>
#include <hotaru/engine.h>
#include <hotaru/view.h>
#include <hotaru/camera.h>
#include <memory>

TEST(TriangleTest, Display) 
{
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    GLFWwindow* window = glfwCreateWindow(800, 600, "Test", nullptr, nullptr);

    hotaru::AppConfig config; 
    std::unique_ptr<hotaru::Engine> engine = hotaru::Engine::Create(config);

    hotaru::Scene* scene = engine->CreateScene();
    hotaru::Entity* triangle = scene->AddEntity();
    hotaru::Renderer* renderer = engine->CreateRenderer();

    hotaru::NativeSurfaceDesc surface_desc;
    hotaru::View* view = engine->CreateView(surface_desc);
    hotaru::Camera* camera = view->GetCamera();

    while (!glfwWindowShouldClose(window)) {

        if (renderer->BeginFrame()) {
            renderer->Render(scene);
            renderer->EndFrame();
        }
    }


    SUCCEED();
}