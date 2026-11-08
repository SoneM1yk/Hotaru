#pragma once

#include "backend/swapchain.h"

#include <hotaru/config.h>
#include <hotaru/result.h>

#include <memory>

#include <volk.h>

namespace hotaru {

class VulkanContext;

class VulkanSwapchain final : public Swapchain
{
public:
    struct ImageState
    {
        VkImage image = VK_NULL_HANDLE;
        VkImageView view = VK_NULL_HANDLE;
        VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
    };

    struct FrameSynchronization
    {
        VkSemaphore image_available = VK_NULL_HANDLE;
        VkSemaphore render_finished = VK_NULL_HANDLE;
        VkFence in_flight_fence = VK_NULL_HANDLE;
    };

    static Result<std::unique_ptr<Swapchain>> Create(
        std::shared_ptr<VulkanContext> context,
        const NativeSurfaceDesc& desc);

    ~VulkanSwapchain() override;

    VulkanSwapchain(const VulkanSwapchain&) = delete;
    VulkanSwapchain& operator=(const VulkanSwapchain&) = delete;
    VulkanSwapchain(VulkanSwapchain&&) = delete;
    VulkanSwapchain& operator=(VulkanSwapchain&&) = delete;

    Status Resize(const Extent& extent) override;
    
    Result<Frame> AcquireFrame() override;
    Status Present(const Frame& frame) override;

    [[nodiscard]] Result<ImageState> GetImageState(
        const Frame& frame) const;
    Status SetImageLayout(const Frame& frame, VkImageLayout layout);
    [[nodiscard]] Result<FrameSynchronization> GetSynchronization(
        const Frame& frame) const;

private:
    VulkanSwapchain(
        std::shared_ptr<VulkanContext> context,
        const NativeSurfaceDesc& desc);

    Status Init();
    Status CreateSurface();
    Status GenerateImageViews();
    Status PickSurfaceFormat();
    Status PickPresentMode();
    Status ValidateFrame(const Frame& frame) const;

    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace hotaru
