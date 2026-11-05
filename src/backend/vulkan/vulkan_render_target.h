#pragma once

#include <volk.h>

namespace hotaru {

struct VulkanRenderTarget
{
    VkImage image = VK_NULL_HANDLE;
    VkImageView image_view = VK_NULL_HANDLE;
    VkFormat format = VK_FORMAT_UNDEFINED;
    VkImageLayout initial_layout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkImageLayout final_layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    VkExtent2D extent{};
};

} // namespace hotaru
