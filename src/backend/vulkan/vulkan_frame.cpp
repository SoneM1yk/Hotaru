#include "vulkan_frame.h"

namespace hotaru {

VulkanFrame::~VulkanFrame()
{
    // The swapchain owns all native resources.
}

RenderBackend VulkanFrame::Backend() const noexcept
{
    return RenderBackend::VULKAN;
}

const FrameDesc& VulkanFrame::Desc() const noexcept
{
    return desc_;
}

uint32_t VulkanFrame::ImageIndex() const noexcept
{
    return image_index_;
}

void VulkanFrame::Initialize(
    VkImage image,
    VkImageView image_view,
    const FrameDesc& desc,
    uint32_t image_index)
{
    image_ = image;
    image_view_ = image_view;
    desc_ = desc;
    image_index_ = image_index;
}

void VulkanFrame::SetImageLayout(VkImageLayout layout) noexcept
{
    image_layout_ = layout;
}

VkImage VulkanFrame::Image() const noexcept
{
    return image_;
}

VkImageView VulkanFrame::ImageView() const noexcept
{
    return image_view_;
}

VkImageLayout VulkanFrame::ImageLayout() const noexcept
{
    return image_layout_;
}

VkSemaphore VulkanFrame::ImageAvailable() const noexcept
{
    return image_available_;
}

VkSemaphore VulkanFrame::RenderFinished() const noexcept
{
    return render_finished_;
}

VkFence VulkanFrame::InFlightFence() const noexcept
{
    return in_flight_fence_;
}

void VulkanFrame::SetSynchronization(
    VkSemaphore image_available,
    VkSemaphore render_finished,
    VkFence in_flight_fence) noexcept
{
    image_available_ = image_available;
    render_finished_ = render_finished;
    in_flight_fence_ = in_flight_fence;
}

} // namespace hotaru
