#pragma once

#include "backend/swapchain.h"

#include <hotaru/config.h>
#include <hotaru/result.h>

#include <cstdint>
#include <memory>
#include <vector>

#include <volk.h>

namespace hotaru {

class VulkanContext;
class CommandList;

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

    Result<SwapchainFrame> AcquireNextImage() override;
    Status Submit(const CommandList& command_list) override;
    Status WaitIdle() override;
    void AbandonFrame() override;
    Status Present() override;
    Status Resize(const Extent& extent) override;

    [[nodiscard]] Extent GetExtent() const noexcept override;
    [[nodiscard]] ColorFormat GetColorFormat() const noexcept override;

    [[nodiscard]] VkImageView CurrentImageView() const noexcept;
    [[nodiscard]] VkImage CurrentImage() const noexcept;
    [[nodiscard]] VkFormat NativeColorFormat() const noexcept;
    [[nodiscard]] VkImageLayout CurrentImageLayout() const noexcept;
    [[nodiscard]] VkSemaphore ImageAvailableSemaphore() const noexcept;
    [[nodiscard]] VkSemaphore RenderFinishedSemaphore() const noexcept;
    [[nodiscard]] VkFence InFlightFence() const noexcept;
    [[nodiscard]] uint32_t CurrentImageIndex() const noexcept;

private:
    VulkanSwapchain(
        std::shared_ptr<VulkanContext> context,
        const NativeSurfaceDesc& desc);

    Status Init();
    Status CreateSurface();
    Status CreateSwapchain();
    Status CreateImageViews(
        const std::vector<VkImage>& images,
        std::vector<VkImageView>& image_views);
    Status RecreateSwapchain();
    Status CreateSyncObjects();
    Status PickSurfaceFormat();
    Status PickPresentMode();
    void DestroySwapchainResources();
    void DestroySyncObjects();

    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace hotaru
