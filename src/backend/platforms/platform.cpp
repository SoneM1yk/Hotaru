#include "platform.h"

#include "platform_linux.h"
#include "../vulkan_driver.h"

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_wayland.h>

namespace Hotaru {

Platform::Platform()
{}

Platform::~Platform()
{}

std::unique_ptr<Platform> Platform::Create(const PlatformConfig& config)
{
    return std::make_unique<PlatformLinux>();
}

Driver* Platform::CreateDriver()
{
    std::vector<const char*> exts = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME
    };

    driver_ = std::make_unique<VulkanDriver>(exts);
    driver_->Init();
    return driver_.get();
}

}