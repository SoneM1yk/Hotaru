#include "vulkan_swapchain.h"

#include "vulkan_context.h"
#include "vulkan_command_list.h"

#include <hotaru/config.h>
#include <hotaru/result.h>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

#include <spdlog/spdlog.h>
#include <vulkan/vulkan_core.h>

namespace hotaru {

namespace {

ColorFormat ToColorFormat(VkFormat format) noexcept
{
    switch (format) {
        case VK_FORMAT_B8G8R8A8_UNORM:
            return ColorFormat::BGRA8_UNORM;
        case VK_FORMAT_R8G8B8A8_UNORM:
            return ColorFormat::RGBA8_UNORM;
        case VK_FORMAT_B8G8R8A8_SRGB:
            return ColorFormat::BGRA8_SRGB;
        case VK_FORMAT_R8G8B8A8_SRGB:
            return ColorFormat::RGBA8_SRGB;
        default:
            return ColorFormat::UNDEFINED;
    }
}

} // namespace

struct VulkanSwapchain::Impl
{
    Impl(std::shared_ptr<VulkanContext> context,
         const NativeSurfaceDesc& desc)
        : context(std::move(context))
        , surface_desc(desc)
    {}

    ~Impl()
    {
        vkQueueWaitIdle(context->GraphicsQueue());

        for (const FrameSync& sync : frame_sync) {
            if (sync.in_flight_fence != VK_NULL_HANDLE) {
                vkWaitForFences(context->Device(), 1,
                    &sync.in_flight_fence, VK_TRUE,
                    std::numeric_limits<uint64_t>::max());
                vkDestroyFence(
                    context->Device(), sync.in_flight_fence, nullptr);
            }
            if (sync.image_available_sem != VK_NULL_HANDLE) {
                vkDestroySemaphore(
                    context->Device(), sync.image_available_sem, nullptr);
            }
        }
        for (VkSemaphore semaphore : render_finished_sems) {
            if (semaphore != VK_NULL_HANDLE) {
                vkDestroySemaphore(context->Device(), semaphore, nullptr);
            }
        }
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
    std::vector<VkImageLayout> image_layouts;

    VkSurfaceFormatKHR format{};
    VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR;
    VkExtent2D extent{};
    uint32_t present_queue_family = VK_QUEUE_FAMILY_IGNORED;

    struct FrameSync
    {
        VkSemaphore image_available_sem = VK_NULL_HANDLE;
        VkFence in_flight_fence = VK_NULL_HANDLE;
    };

    static constexpr size_t kFramesInFlight = 2;
    std::vector<FrameSync> frame_sync;
    std::vector<VkSemaphore> render_finished_sems;
    size_t current_frame = 0;

    std::optional<Frame> acquired_frame;
    FrameSynchronization acquired_sync;
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

    if (Status status = CreateSurface()
        .and_then([this] { return PickSurfaceFormat(); })
        .and_then([this] { return PickPresentMode(); }); !status) {
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

    const ColorFormat color_format = ToColorFormat(impl_->format.format);
    if (color_format == ColorFormat::UNDEFINED) {
        return Failure(ErrorCode::Unsupported,
                       "The swapchain image format is not supported");
    }

    SPDLOG_INFO(
        "Vulkan swapchain initialized ({}x{}, {} images, present mode {})",
        impl_->extent.width, impl_->extent.height, impl_->images.size(),
        static_cast<int>(impl_->present_mode));
    SPDLOG_INFO("Vulkan queues: graphics {}, present {}",
                impl_->context->GraphicsQueueFamily(),
                impl_->present_queue_family);

    VkSemaphoreCreateInfo semaphore_info {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    };
    VkFenceCreateInfo fence_info {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT,
    };

    impl_->frame_sync.resize(Impl::kFramesInFlight);
    impl_->current_frame = 0;
    for (Impl::FrameSync& sync : impl_->frame_sync) {
        if (vkCreateSemaphore(impl_->context->Device(), &semaphore_info,
                nullptr, &sync.image_available_sem) != VK_SUCCESS) {
            return Failure(ErrorCode::InitializationFailed,
                "Failed to create image available semaphore!");
        }
        if (vkCreateFence(impl_->context->Device(), &fence_info, nullptr,
                &sync.in_flight_fence) != VK_SUCCESS) {
            return Failure(ErrorCode::InitializationFailed,
                "Failed to create in-flight fence!");
        }
    }

    impl_->render_finished_sems.resize(impl_->images.size());
    for (VkSemaphore& semaphore : impl_->render_finished_sems) {
        if (vkCreateSemaphore(impl_->context->Device(), &semaphore_info,
                nullptr, &semaphore) != VK_SUCCESS) {
            semaphore = VK_NULL_HANDLE;
            return Failure(ErrorCode::InitializationFailed,
                "Failed to create render finished semaphore!");
        }
    }

    impl_->image_layouts.assign(
        impl_->images.size(), VK_IMAGE_LAYOUT_UNDEFINED);

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

Result<Frame> VulkanSwapchain::AcquireFrame()
{
    if (impl_->acquired_frame.has_value()) {
        return Failure(ErrorCode::InvalidState,
                       "A frame is already acquired");
    }

    Impl::FrameSync& sync = impl_->frame_sync[impl_->current_frame];
    if (vkWaitForFences(impl_->context->Device(), 1,
            &sync.in_flight_fence, VK_TRUE,
            std::numeric_limits<uint64_t>::max()) != VK_SUCCESS) {
        return Failure(ErrorCode::RuntimeError,
                       "Failed to wait for the in-flight fence");
    }

    uint32_t image_index = 0;
    const VkResult result = vkAcquireNextImageKHR(
        impl_->context->Device(), impl_->swapchain,
        std::numeric_limits<uint64_t>::max(), sync.image_available_sem,
        VK_NULL_HANDLE, &image_index);
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        return Failure(ErrorCode::NotReady, "The swapchain is out of date");
    }
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        SPDLOG_ERROR("vkAcquireNextImageKHR failed with VkResult {}",
                     static_cast<int>(result));
        return Failure(ErrorCode::RuntimeError,
                       "Failed to acquire a swapchain image");
    }

    if (vkResetFences(impl_->context->Device(), 1,
            &sync.in_flight_fence) != VK_SUCCESS) {
        return Failure(ErrorCode::RuntimeError,
                       "Failed to reset the in-flight fence");
    }

    impl_->acquired_sync = FrameSynchronization{
        .image_available = sync.image_available_sem,
        .render_finished = impl_->render_finished_sems[image_index],
        .in_flight_fence = sync.in_flight_fence,
    };
    impl_->acquired_frame = Frame{
        .swapchain = this,
        .image_index = image_index,
        .desc = {
            .extent = {
                .width = impl_->extent.width,
                .height = impl_->extent.height,
            },
            .color_format = ToColorFormat(impl_->format.format),
        },
    };
    return *impl_->acquired_frame;
}

Status VulkanSwapchain::Present(const Frame& frame)
{
    if (Status status = ValidateFrame(frame); !status) {
        return status;
    }

    const VkSemaphore render_finished = impl_->acquired_sync.render_finished;
    const uint32_t image_index = frame.image_index;
    VkPresentInfoKHR present_info {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &render_finished,
        .swapchainCount = 1,
        .pSwapchains = &impl_->swapchain,
        .pImageIndices = &image_index,
        .pResults = nullptr,
    };
    const VkResult result = vkQueuePresentKHR(
        impl_->context->GraphicsQueue(), &present_info);
    impl_->acquired_frame.reset();
    impl_->acquired_sync = {};
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        return Failure(ErrorCode::RuntimeError,
            "Failed to present the frame!");
    }
    impl_->current_frame =
        (impl_->current_frame + 1) % Impl::kFramesInFlight;
    return {};
}

Status VulkanSwapchain::ValidateFrame(const Frame& frame) const
{
    if (frame.swapchain != this) {
        return Failure(ErrorCode::InvalidArgument,
                       "Frame does not belong to this swapchain");
    }
    if (!impl_->acquired_frame.has_value() ||
        frame.image_index != impl_->acquired_frame->image_index) {
        return Failure(ErrorCode::InvalidState,
                       "Frame is not currently acquired");
    }
    if (frame.image_index >= impl_->images.size()) {
        return Failure(ErrorCode::InvalidArgument,
                       "Frame image index is out of range");
    }
    return {};
}

Result<VulkanSwapchain::ImageState> VulkanSwapchain::GetImageState(
    const Frame& frame) const
{
    if (Status status = ValidateFrame(frame); !status) {
        return std::unexpected(std::move(status.error()));
    }

    return ImageState{
        .image = impl_->images[frame.image_index],
        .view = impl_->image_views[frame.image_index],
        .layout = impl_->image_layouts[frame.image_index],
    };
}

Status VulkanSwapchain::SetImageLayout(
    const Frame& frame, VkImageLayout layout)
{
    if (Status status = ValidateFrame(frame); !status) {
        return status;
    }

    impl_->image_layouts[frame.image_index] = layout;
    return {};
}

Result<VulkanSwapchain::FrameSynchronization>
VulkanSwapchain::GetSynchronization(const Frame& frame) const
{
    if (Status status = ValidateFrame(frame); !status) {
        return std::unexpected(std::move(status.error()));
    }

    return impl_->acquired_sync;
}

} // namespace hotaru
