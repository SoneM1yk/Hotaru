#include <gtest/gtest.h>

#define GLFW_INCLUDE_NONE
#define GLFW_EXPOSE_NATIVE_WAYLAND
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include <hotaru/engine.h>
#include <hotaru/renderer.h>

#include <chrono>

TEST(VulkanTriangleTest, DrawTriangle)
{
    glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_WAYLAND);
    if (!glfwInit()) {
        GTEST_SKIP() << "Wayland is unavailable in this environment";
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    GLFWwindow* window = glfwCreateWindow(800, 600, "Test", nullptr, nullptr);
    if (window == nullptr) {
        glfwTerminate();
        GTEST_SKIP() << "Could not create a Wayland window";
    }

    glfwShowWindow(window);
    glfwPollEvents();
    glfwWaitEventsTimeout(0.5);

    {
        hotaru::AppConfig config {
            .backend = hotaru::RenderBackend::VULKAN,
            .surface_type = hotaru::SurfaceType::WAYLAND,
        };

        auto engine = hotaru::Engine::Create(config);
        ASSERT_NE(engine, nullptr);

        const hotaru::WaylandSurfaceDesc wayland_desc{
            .extent = {
                .width = 800,
                .height = 600,
            },
            .surface = glfwGetWaylandWindow(window),
            .display = glfwGetWaylandDisplay(),
        };
        ASSERT_NE(wayland_desc.surface, nullptr);
        ASSERT_NE(wayland_desc.display, nullptr);

        hotaru::NativeSurfaceDesc desc = wayland_desc;

        hotaru::View* view = engine->CreateView(desc);
        ASSERT_NE(view, nullptr);
        hotaru::Renderer* renderer = engine->CreateRenderer();
        ASSERT_NE(renderer, nullptr);

        constexpr auto kDisplayDuration = std::chrono::seconds(3);
        const auto deadline = std::chrono::steady_clock::now() +
            kDisplayDuration;
        size_t frame_index = 0;
        while (!glfwWindowShouldClose(window) &&
            std::chrono::steady_clock::now() < deadline) {
            glfwPollEvents();
            SCOPED_TRACE("frame " + std::to_string(frame_index));
            ASSERT_TRUE(renderer->BeginFrame(view));
            renderer->Render();
            renderer->EndFrame();
            ++frame_index;
            glfwWaitEventsTimeout(1.0 / 60.0);
        }
    }

    glfwDestroyWindow(window);
    glfwTerminate();
}
