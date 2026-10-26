#pragma once

#include "backend/frame.h"

#include <volk.h>

namespace hotaru {

class VulkanFrame final : public Frame
{
public:
    VulkanFrame() = default;
    ~VulkanFrame() override;

    VulkanFrame(const VulkanFrame&) = delete;
    VulkanFrame& operator=(const VulkanFrame&) = delete;

    RenderBackend Backend() const noexcept override;
    const FrameDesc& Desc() const noexcept override;
    uint32_t ImageIndex() const noexcept override;

    void Initialize(
        VkImage image,
        VkImageView image_view,
        const FrameDesc& desc,
        uint32_t image_index);
    void SetImageLayout(VkImageLayout layout) noexcept;

    [[nodiscard]] VkImage Image() const noexcept;
    [[nodiscard]] VkImageView ImageView() const noexcept;
    [[nodiscard]] VkImageLayout ImageLayout() const noexcept;
    [[nodiscard]] VkSemaphore ImageAvailable() const noexcept;
    [[nodiscard]] VkSemaphore RenderFinished() const noexcept;
    [[nodiscard]] VkFence InFlightFence() const noexcept;
    void SetSynchronization(
        VkSemaphore image_available,
        VkSemaphore render_finished,
        VkFence in_flight_fence) noexcept;

private:
    VkImage image_ = VK_NULL_HANDLE;
    VkImageView image_view_ = VK_NULL_HANDLE;
    VkImageLayout image_layout_ = VK_IMAGE_LAYOUT_UNDEFINED;
    FrameDesc desc_{};
    VkSemaphore image_available_ = VK_NULL_HANDLE;
    VkSemaphore render_finished_ = VK_NULL_HANDLE;
    VkFence in_flight_fence_ = VK_NULL_HANDLE;
    uint32_t image_index_ = 0;
};

} // namespace hotaru
