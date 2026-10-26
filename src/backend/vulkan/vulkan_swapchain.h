#pragma once

#include "backend/swapchain.h"

#include <hotaru/config.h>
#include <hotaru/result.h>

#include <memory>

namespace hotaru {

class VulkanContext;
class Frame;

class VulkanSwapchain final : public Swapchain
{
public:
    static Result<std::unique_ptr<Swapchain>> Create(
        std::shared_ptr<VulkanContext> context,
        const NativeSurfaceDesc& desc);

    ~VulkanSwapchain() override;

    VulkanSwapchain(const VulkanSwapchain&) = delete;
    VulkanSwapchain& operator=(const VulkanSwapchain&) = delete;
    VulkanSwapchain(VulkanSwapchain&&) = delete;
    VulkanSwapchain& operator=(VulkanSwapchain&&) = delete;

    Status Resize(const Extent& extent) override;
    
    Result<Frame*> AcquireFrame() override;
    Status Present(Frame& frame) override;

private:
    VulkanSwapchain(
        std::shared_ptr<VulkanContext> context,
        const NativeSurfaceDesc& desc);

    Status Init();
    Status CreateSurface();
    Status GenerateImageViews();
    Status PickSurfaceFormat();
    Status PickPresentMode();

    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace hotaru
