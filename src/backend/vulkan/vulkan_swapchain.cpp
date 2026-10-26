#include "vulkan_swapchain.h"

#include "vulkan_context.h"

#include <hotaru/config.h>
#include <hotaru/result.h>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <memory>
#include <utility>
#include <variant>
#include <vector>

#include <spdlog/spdlog.h>

namespace hotaru {

struct VulkanSwapchain::Impl
{
    Impl(std::shared_ptr<VulkanContext> context,
         const NativeSurfaceDesc& desc)
        : context(std::move(context))
        , surface_desc(desc)
    {}

    ~Impl()
    {
        for (const auto& image_view : image_views) {
            if (image_view != VK_NULL_HANDLE) {
                vkDestroyImageView(context->Device(), image_view, nullptr);
            }
        }
        if (swapchain != VK_NULL_HANDLE) {
            vkDestroySwapchainKHR(context->Device(), swapchain, nullptr);
        }
        if (surface != VK_NULL_HANDLE) {
            vkDestroySurfaceKHR(context->Instance(), surface, nullptr);
        }
    }

    std::shared_ptr<VulkanContext> context;
    NativeSurfaceDesc surface_desc;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    std::vector<VkImage> images;
    std::vector<VkImageView> image_views;

    VkSurfaceFormatKHR format{};
    VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR;
    VkExtent2D extent{};
    uint32_t present_queue_family = VK_QUEUE_FAMILY_IGNORED;
};

VulkanSwapchain::VulkanSwapchain(
    std::shared_ptr<VulkanContext> context,
    const NativeSurfaceDesc& desc)
    : impl_(std::make_unique<Impl>(std::move(context), desc))
{}

VulkanSwapchain::~VulkanSwapchain()
{}

Result<std::unique_ptr<Swapchain>> VulkanSwapchain::Create(
    std::shared_ptr<VulkanContext> context,
    const NativeSurfaceDesc& desc)
{
    std::unique_ptr<VulkanSwapchain> swapchain(
        new VulkanSwapchain(std::move(context), desc));

    if (Status status = swapchain->Init(); !status) {
        return std::unexpected(std::move(status.error()));
    }

    std::unique_ptr<Swapchain> result(std::move(swapchain));
    return result;
}

Status VulkanSwapchain::Init()
{
    if (impl_->context->RequestedSurfaceType() != SurfaceType::WAYLAND) {
        return Failure(ErrorCode::Unsupported,
                       "The active Vulkan driver was not configured for Wayland");
    }
    if (!std::holds_alternative<WaylandSurfaceDesc>(impl_->surface_desc)) {
        return Failure(ErrorCode::InvalidArgument,
                       "Expected a Wayland surface description");
    }

    if (Status status = CreateSurface(); !status) {
        return status;
    }
    if (Status status = PickSurfaceFormat(); !status) {
        return status;
    }
    if (Status status = PickPresentMode(); !status) {
        return status;
    }

    VkSurfaceCapabilitiesKHR capabilities{};
    VkResult vk_result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
        impl_->context->PhysicalDevice(), impl_->surface, &capabilities);
    if (vk_result != VK_SUCCESS) {
        return Failure(ErrorCode::InitializationFailed,
                       "Failed to query Vulkan surface capabilities");
    }

    const auto& desc = std::get<WaylandSurfaceDesc>(impl_->surface_desc);
    if (capabilities.currentExtent.width !=
        std::numeric_limits<uint32_t>::max()) {
        impl_->extent = capabilities.currentExtent;
    } else {
        impl_->extent.width = std::clamp(
            desc.extent.width,
            capabilities.minImageExtent.width,
            capabilities.maxImageExtent.width);
        impl_->extent.height = std::clamp(
            desc.extent.height,
            capabilities.minImageExtent.height,
            capabilities.maxImageExtent.height);
    }

    impl_->present_queue_family = VK_QUEUE_FAMILY_IGNORED;
    for (const VulkanContext::QueueInfo& queue : impl_->context->Queues()) {
        VkBool32 present_supported = VK_FALSE;
        vk_result = vkGetPhysicalDeviceSurfaceSupportKHR(
            impl_->context->PhysicalDevice(), queue.family_index,
            impl_->surface, &present_supported);
        if (vk_result == VK_SUCCESS && present_supported == VK_TRUE) {
            impl_->present_queue_family = queue.family_index;
            break;
        }
    }
    if (impl_->present_queue_family == VK_QUEUE_FAMILY_IGNORED) {
        return Failure(
            ErrorCode::Unsupported,
            "No Vulkan queue family supports presentation to this surface");
    }

    uint32_t image_count = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && image_count > capabilities.maxImageCount) {
        image_count = capabilities.maxImageCount;
    }

    const uint32_t queue_families[] = {
        impl_->context->GraphicsQueueFamily(),
        impl_->present_queue_family,
    };
    const bool separate_queues =
        impl_->context->GraphicsQueueFamily() != impl_->present_queue_family;

    VkCompositeAlphaFlagBitsKHR composite_alpha =
        VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    if ((capabilities.supportedCompositeAlpha &
         VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR) == 0) {
        const VkCompositeAlphaFlagBitsKHR candidates[] = {
            VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
            VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
            VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
        };
        for (VkCompositeAlphaFlagBitsKHR candidate : candidates) {
            if ((capabilities.supportedCompositeAlpha & candidate) != 0) {
                composite_alpha = candidate;
                break;
            }
        }
    }

    VkSwapchainCreateInfoKHR create_info {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = impl_->surface,

        .minImageCount = image_count,
        .imageFormat = impl_->format.format,
        .imageColorSpace = impl_->format.colorSpace,
        .imageExtent = impl_->extent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,

        .imageSharingMode = separate_queues
            ? VK_SHARING_MODE_CONCURRENT
            : VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = separate_queues ? 2u : 0u,
        .pQueueFamilyIndices = separate_queues ? queue_families : nullptr,

        .preTransform = capabilities.currentTransform,
        .compositeAlpha = composite_alpha,
        .presentMode = impl_->present_mode,
        .clipped = VK_TRUE,
        .oldSwapchain = nullptr
    };

    vk_result = vkCreateSwapchainKHR(
        impl_->context->Device(), &create_info, nullptr, &impl_->swapchain);
    if (vk_result != VK_SUCCESS) {
        impl_->swapchain = VK_NULL_HANDLE;
        return Failure(ErrorCode::InitializationFailed,
                       "Failed to create the Vulkan swapchain");
    }

    vk_result = vkGetSwapchainImagesKHR(
        impl_->context->Device(), impl_->swapchain, &image_count, nullptr);
    if (vk_result != VK_SUCCESS || image_count == 0) {
        return Failure(ErrorCode::InitializationFailed,
                       "Failed to query Vulkan swapchain images");
    }

    impl_->images.resize(image_count);
    vk_result = vkGetSwapchainImagesKHR(
        impl_->context->Device(), impl_->swapchain, &image_count,
        impl_->images.data());
    if (vk_result != VK_SUCCESS) {
        return Failure(ErrorCode::InitializationFailed,
                       "Failed to retrieve Vulkan swapchain images");
    }

    if (Status status = GenerateImageViews(); !status) {
        return status;
    }

    SPDLOG_INFO("Vulkan swapchain initialized ({}x{})",
                impl_->extent.width, impl_->extent.height);
    return {};
}

Status VulkanSwapchain::GenerateImageViews()
{
    impl_->image_views.resize(impl_->images.size());

    for (size_t i = 0; i < impl_->images.size(); ++i) {
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
                impl_->context->Device(), &create_info,
                nullptr, &impl_->image_views[i]) != VK_SUCCESS) {
            impl_->image_views[i] = VK_NULL_HANDLE;
            return Failure(ErrorCode::InitializationFailed,
                           "Create Image View Failed!");
        }
    }

    return {};
}

Status VulkanSwapchain::CreateSurface()
{
#if defined(VK_USE_PLATFORM_WAYLAND_KHR)
    const auto& desc = std::get<WaylandSurfaceDesc>(impl_->surface_desc);
    VkWaylandSurfaceCreateInfoKHR create_info {
        .sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR,
        .display = static_cast<wl_display*>(desc.display),
        .surface = static_cast<wl_surface*>(desc.surface),
    };

    const VkResult result = vkCreateWaylandSurfaceKHR(
        impl_->context->Instance(), &create_info, nullptr, &impl_->surface);
    if (result != VK_SUCCESS) {
        impl_->surface = VK_NULL_HANDLE;
        return Failure(ErrorCode::InitializationFailed,
                       "Failed to create the Wayland Vulkan surface");
    }

    return {};
#else
    return Failure(ErrorCode::Unsupported,
                   "This build does not support the Wayland Vulkan surface");
#endif
}

Status VulkanSwapchain::PickSurfaceFormat()
{
    uint32_t format_count = 0;
    VkResult vk_result = vkGetPhysicalDeviceSurfaceFormatsKHR(
        impl_->context->PhysicalDevice(), impl_->surface, &format_count,
        nullptr);
    if (vk_result != VK_SUCCESS || format_count == 0) {
        return Failure(ErrorCode::Unsupported,
                       "The Vulkan surface exposes no supported formats");
    }

    std::vector<VkSurfaceFormatKHR> formats(format_count);
    vk_result = vkGetPhysicalDeviceSurfaceFormatsKHR(
        impl_->context->PhysicalDevice(), impl_->surface, &format_count,
        formats.data());
    if (vk_result != VK_SUCCESS) {
        return Failure(ErrorCode::InitializationFailed,
                       "Failed to query Vulkan surface formats");
    }

    if (formats.size() == 1 && formats[0].format == VK_FORMAT_UNDEFINED) {
        impl_->format = {
            VK_FORMAT_B8G8R8A8_UNORM,
            VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
        };
        return {};
    }

    for (const VkSurfaceFormatKHR& format : formats) {
        if (format.format == VK_FORMAT_B8G8R8A8_UNORM &&
            format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            impl_->format = format;
            return {};
        }
    }

    impl_->format = formats.front();
    return {};
}

Status VulkanSwapchain::PickPresentMode()
{
    uint32_t present_mode_count = 0;
    VkResult vk_result = vkGetPhysicalDeviceSurfacePresentModesKHR(
        impl_->context->PhysicalDevice(), impl_->surface, &present_mode_count,
        nullptr);
    if (vk_result != VK_SUCCESS || present_mode_count == 0) {
        return Failure(ErrorCode::Unsupported,
                       "The Vulkan surface exposes no supported present modes");
    }

    std::vector<VkPresentModeKHR> present_modes(present_mode_count);
    vk_result = vkGetPhysicalDeviceSurfacePresentModesKHR(
        impl_->context->PhysicalDevice(), impl_->surface, &present_mode_count,
        present_modes.data());
    if (vk_result != VK_SUCCESS) {
        return Failure(ErrorCode::InitializationFailed,
                       "Failed to query Vulkan present modes");
    }

    impl_->present_mode = VK_PRESENT_MODE_FIFO_KHR;
    for (VkPresentModeKHR present_mode : present_modes) {
        if (present_mode == VK_PRESENT_MODE_MAILBOX_KHR) {
            impl_->present_mode = present_mode;
            break;
        }
        if (present_mode == VK_PRESENT_MODE_IMMEDIATE_KHR) {
            impl_->present_mode = present_mode;
        }
    }

    return {};
}

Status VulkanSwapchain::Resize(const Extent& extent)
{
    if (extent.width == impl_->extent.width &&
        extent.height == impl_->extent.height) {
        return {};
    }

    return Failure(ErrorCode::Unsupported,
                   "Vulkan swapchain resizing is not implemented yet");
}

} // namespace hotaru
