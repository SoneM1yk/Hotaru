#pragma once

#include "backend/swapchain.h"
#include "hotaru/config.h"
#include "result.h"

#include <memory>
#include <volk.h>

namespace hotaru {

class VulkanSwapchain : public Swapchain 
{
public:
    VulkanSwapchain(
        const NativeSurfaceDesc& desc,
        VkInstance instance,
        VkDevice device);
        
    virtual ~VulkanSwapchain();

    Result Init(
        VkSurfaceCapabilitiesKHR capabilities,
        VkSurfaceFormatKHR format,
        VkPresentModeKHR present_mode);

    VkSurfaceKHR Surface() const;
    Result SetSurfaceFormat(VkSurfaceFormatKHR format);
    Result SetPresentMode();

    Result Resize(const Extent& extent) override;

private:
    void InitSurface();
    Result GenerateImageViews();

    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}