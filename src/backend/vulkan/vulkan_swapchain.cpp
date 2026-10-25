#include "vulkan_swapchain.h"
#include "result.h"
#include <hotaru/config.h>

#include <variant>
#include <limits>

#include <spdlog/spdlog.h>

namespace hotaru {

struct VulkanSwapchain::Impl
{
    ~Impl() 
    {
        for (const auto& image_view : image_views) {
            if (image_view != VK_NULL_HANDLE) {
                vkDestroyImageView(device, image_view, nullptr);
            }
        }
        if (swapchain != VK_NULL_HANDLE) {
            vkDestroySwapchainKHR(device, swapchain, nullptr);
        }
        if (surface != VK_NULL_HANDLE) {
            vkDestroySurfaceKHR(instance, surface, nullptr);
        }
    }

    NativeSurfaceDesc surface_desc;
    VkInstance instance = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    std::vector<VkImage> images;
    std::vector<VkImageView> image_views;

    VkSurfaceFormatKHR format;
    VkPresentModeKHR present_mode;
    VkSurfaceCapabilitiesKHR capabilities;  

    Extent extent;
};

VulkanSwapchain::VulkanSwapchain(
    const NativeSurfaceDesc& desc,
    VkInstance instance,
    VkDevice device) :
    impl_(std::make_unique<Impl>())
{
    impl_->surface_desc = desc;
    impl_->instance = instance;
    impl_->device = device;

    InitSurface();
}

VulkanSwapchain::~VulkanSwapchain()
{}

Result VulkanSwapchain::Init(
        VkSurfaceCapabilitiesKHR capabilities,
        VkSurfaceFormatKHR format,
        VkPresentModeKHR present_mode)
{
    impl_->format = format;

    VkExtent2D extent;
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        extent = capabilities.currentExtent;
    } else {
        extent = {impl_->extent.width, impl_->extent.height};
        extent.width = 
            std::max(capabilities.minImageExtent.width, 
                std::min(capabilities.maxImageExtent.width, extent.width));
        extent.height = 
            std::max(capabilities.minImageExtent.height, 
                std::min(capabilities.maxImageExtent.height, extent.height));
    }
    
    uint32_t image_count = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && image_count > capabilities.maxImageCount) {
        image_count = capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR create_info {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = impl_->surface,
        
        .minImageCount = image_count,
        .imageFormat = format.format,
        .imageColorSpace = format.colorSpace,
        .imageExtent = extent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,

        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr,

        .preTransform = capabilities.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = present_mode,
        .clipped = VK_TRUE,
        .oldSwapchain = nullptr
    };

    if (vkCreateSwapchainKHR(impl_->device, &create_info, 
            nullptr, &impl_->swapchain) != VK_SUCCESS) {
        return Result::Failure(ResultType::InitializationFailed, 
            "Swapchain Initialize Failed!");
    }

    vkGetSwapchainImagesKHR(
        impl_->device, impl_->swapchain, &image_count, nullptr);
    impl_->images.resize(image_count);
    vkGetSwapchainImagesKHR(
        impl_->device, impl_->swapchain, &image_count, impl_->images.data());

    GenerateImageViews();

    SPDLOG_INFO("Swapchain Initialized!");
    return Result::Ok();
}

Result VulkanSwapchain::GenerateImageViews()
{
    impl_->image_views.resize(impl_->images.size());

    for (int i = 0; i < impl_->images.size(); i++) {
        VkImageViewCreateInfo create_info {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = impl_->images[i],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = impl_->format.format,
            .components = { 
                .r = VK_COMPONENT_SWIZZLE_IDENTITY, 
                .g = VK_COMPONENT_SWIZZLE_IDENTITY, 
                .b = VK_COMPONENT_SWIZZLE_IDENTITY, 
                .a = VK_COMPONENT_SWIZZLE_IDENTITY, 
            },
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
        };

        if (vkCreateImageView(
                impl_->device, &create_info, 
                nullptr, &impl_->image_views[i]) != VK_SUCCESS) {
            return Result::Failure(ResultType::InitializationFailed, 
                "Create Image View Failed!");
        }
    }

    return Result::Ok();
}

void VulkanSwapchain::InitSurface() 
{
#ifdef __linux__ 
    if (std::holds_alternative<WaylandSurfaceDesc>(impl_->surface_desc)) {
        const auto& desc = std::get<WaylandSurfaceDesc>(impl_->surface_desc); 
        VkWaylandSurfaceCreateInfoKHR create_info {
            .sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR,
            .display = static_cast<wl_display*>(desc.display),
            .surface = static_cast<wl_surface*>(desc.surface),
        };

        if (vkCreateWaylandSurfaceKHR(impl_->instance, &create_info,
            nullptr, &impl_->surface) != VK_SUCCESS) {
            return;
        }

        impl_->extent = desc.extent;
    }
#else 
#endif

    SPDLOG_INFO("Surface Create Succeed!");
}

VkSurfaceKHR VulkanSwapchain::Surface() const
{
    return impl_->surface;
}

Result VulkanSwapchain::Resize(const Extent& extent) 
{
    return Result::Ok();
}

}