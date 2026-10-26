#include <hotaru/config.h>
#include <hotaru/engine.h>
#include <hotaru/result.h>
#include <hotaru/view.h>

#include <gtest/gtest.h>

#define GLFW_INCLUDE_NONE
#define GLFW_EXPOSE_NATIVE_WAYLAND
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>


#include <memory>

TEST(VulkanInitTest, HeadlessInitializesDriver)
{
    hotaru::AppConfig config;
    config.surface_type = hotaru::SurfaceType::WAYLAND;

    const std::unique_ptr<hotaru::Engine> engine =
        hotaru::Engine::Create(config);

    EXPECT_NE(engine, nullptr);
}

TEST(VulkanInitTest, MissingPlatformSurfaceExtensionFailsClearly)
{
    hotaru::AppConfig config;
    config.surface_type = hotaru::SurfaceType::WIN32;

    const std::unique_ptr<hotaru::Engine> engine =
        hotaru::Engine::Create(config);

    if (engine) {
        GTEST_SKIP() << "Host Vulkan loader supports VK_KHR_win32_surface";
    }

    EXPECT_EQ(engine, nullptr);
}

TEST(VulkanInitTest, CreateSwapchain)
{
    glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_WAYLAND);
    if (!glfwInit()) {
        GTEST_SKIP() << "Wayland is unavailable in this environment";
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    GLFWwindow* window = glfwCreateWindow(
        800, 600, "GLFW Wayland Test", nullptr, nullptr);
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
            .extent = {
                .width = 800,
                .height = 600,
            },
            .surface = glfwGetWaylandWindow(window),
            .display = glfwGetWaylandDisplay(),
        };
        hotaru::View* view = engine->CreateView(desc);

        EXPECT_NE(view, nullptr);
        EXPECT_TRUE(view->Resize({800, 600}).has_value());
    }

    glfwDestroyWindow(window);
    glfwTerminate();
}
