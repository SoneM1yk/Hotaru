#include <hotaru/config.h>
#include <hotaru/engine.h>

#include <gtest/gtest.h>

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
