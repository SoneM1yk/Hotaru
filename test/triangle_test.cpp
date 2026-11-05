#include <hotaru/config.h>
#include <hotaru/engine.h>
#include <hotaru/renderer.h>
#include <hotaru/view.h>

#include <gtest/gtest.h>

#define GLFW_INCLUDE_NONE
#define GLFW_EXPOSE_NATIVE_WAYLAND
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include <cstdlib>
#include <memory>

namespace {

int FrameLimit()
{
    if (const char* value = std::getenv("HOTARU_TRIANGLE_TEST_FRAMES")) {
        const int frames = std::atoi(value);
        if (frames > 0) {
            return frames;
        }
    }
    return 120;
}

} // namespace

TEST(TriangleTest, RendersTriangleInWindow)
{
    glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_WAYLAND);
    if (glfwInit() != GLFW_TRUE) {
        GTEST_SKIP() << "Wayland/GLFW is unavailable in this environment";
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    GLFWwindow* window = glfwCreateWindow(
        800, 600, "Hotaru triangle", nullptr, nullptr);
    if (window == nullptr) {
        glfwTerminate();
        GTEST_SKIP() << "Could not create a Wayland window";
    }

    {
        hotaru::AppConfig config;
        config.backend = hotaru::RenderBackend::VULKAN;
        config.surface_type = hotaru::SurfaceType::WAYLAND;

        auto engine = hotaru::Engine::Create(config);
        ASSERT_NE(engine, nullptr);

        hotaru::NativeSurfaceDesc desc = hotaru::WaylandSurfaceDesc{
            .extent = {.width = 800, .height = 600},
            .surface = glfwGetWaylandWindow(window),
            .display = glfwGetWaylandDisplay(),
        };
        hotaru::View* view = engine->CreateView(desc);
        ASSERT_NE(view, nullptr);

        hotaru::Renderer* renderer = engine->CreateRenderer();
        ASSERT_NE(renderer, nullptr);

        const int frame_limit = FrameLimit();
        int rendered_frames = 0;
        while (!glfwWindowShouldClose(window) &&
               rendered_frames < frame_limit) {
            glfwPollEvents();
            if (renderer->BeginFrame(view)) {
                renderer->Render();
                renderer->EndFrame();
                ++rendered_frames;
            }
        }

        EXPECT_GT(rendered_frames, 0);
    }

    glfwDestroyWindow(window);
    glfwTerminate();
}
