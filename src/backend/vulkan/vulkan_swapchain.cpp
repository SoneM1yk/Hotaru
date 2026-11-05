#include "vulkan_swapchain.h"

#include "vulkan_command_list.h"
#include "vulkan_context.h"
#include "vulkan_render_target.h"

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

namespace {

constexpr uint32_t kInvalidImageIndex =
    std::numeric_limits<uint32_t>::max();

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
        if (surface != VK_NULL_HANDLE) {
            vkDestroySurfaceKHR(context->Instance(), surface, nullptr);
        }
    }

    std::shared_ptr<VulkanContext> context;
    NativeSurfaceDesc surface_desc;
    Extent requested_extent{};
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    std::vector<VkImage> images;
    std::vector<VkImageView> image_views;
    std::vector<bool> image_initialized;
    std::vector<VkSemaphore> render_finished;

    VkSurfaceFormatKHR format{};
    VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR;
    VkExtent2D extent{};
    uint32_t present_queue_family = VK_QUEUE_FAMILY_IGNORED;
    VkQueue present_queue = VK_NULL_HANDLE;

    VkSemaphore image_available = VK_NULL_HANDLE;
    VkFence in_flight = VK_NULL_HANDLE;
    VulkanRenderTarget current_render_target{};
    uint32_t current_image_index = kInvalidImageIndex;
};

VulkanSwapchain::VulkanSwapchain(
    std::shared_ptr<VulkanContext> context,
    const NativeSurfaceDesc& desc)
    : impl_(std::make_unique<Impl>(std::move(context), desc))
{}

VulkanSwapchain::~VulkanSwapchain()
{
    if (!impl_ || impl_->context->Device() == VK_NULL_HANDLE) {
        return;
    }

    vkDeviceWaitIdle(impl_->context->Device());
    DestroySyncObjects();
    DestroySwapchainResources();
}

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
    impl_->requested_extent =
        std::get<WaylandSurfaceDesc>(impl_->surface_desc).extent;

    if (Status status = CreateSurface(); !status) {
        return status;
    }
    if (Status status = PickSurfaceFormat(); !status) {
        return status;
    }
    if (Status status = PickPresentMode(); !status) {
        return status;
    }

    VkResult vk_result = VK_SUCCESS;
    impl_->present_queue_family = VK_QUEUE_FAMILY_IGNORED;
    for (const VulkanContext::QueueInfo& queue : impl_->context->Queues()) {
        VkBool32 present_supported = VK_FALSE;
        vk_result = vkGetPhysicalDeviceSurfaceSupportKHR(
            impl_->context->PhysicalDevice(), queue.family_index,
            impl_->surface, &present_supported);
        if (vk_result == VK_SUCCESS && present_supported == VK_TRUE) {
            impl_->present_queue_family = queue.family_index;
            impl_->present_queue = queue.queue;
            break;
        }
    }
    if (impl_->present_queue_family == VK_QUEUE_FAMILY_IGNORED) {
        return Failure(
            ErrorCode::Unsupported,
            "No Vulkan queue family supports presentation to this surface");
    }

    if (Status status = CreateSwapchain(); !status) {
        return status;
    }
    if (Status status = CreateSyncObjects(); !status) {
        return status;
    }

    SPDLOG_INFO("Vulkan swapchain initialized ({}x{})",
                impl_->extent.width, impl_->extent.height);
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

Status VulkanSwapchain::CreateSwapchain()
{
    VkSurfaceCapabilitiesKHR capabilities{};
    VkResult vk_result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
        impl_->context->PhysicalDevice(), impl_->surface, &capabilities);
    if (vk_result != VK_SUCCESS) {
        return Failure(ErrorCode::InitializationFailed,
                       "Failed to query Vulkan surface capabilities");
    }

    VkExtent2D new_extent{};
    if (capabilities.currentExtent.width !=
        std::numeric_limits<uint32_t>::max()) {
        new_extent = capabilities.currentExtent;
    } else {
        new_extent.width = std::clamp(
            impl_->requested_extent.width,
            capabilities.minImageExtent.width,
            capabilities.maxImageExtent.width);
        new_extent.height = std::clamp(
            impl_->requested_extent.height,
            capabilities.minImageExtent.height,
            capabilities.maxImageExtent.height);
    }

    uint32_t image_count = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 &&
        image_count > capabilities.maxImageCount) {
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
        .imageExtent = new_extent,
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
        .oldSwapchain = impl_->swapchain,
    };

    VkSwapchainKHR new_swapchain = VK_NULL_HANDLE;
    vk_result = vkCreateSwapchainKHR(
        impl_->context->Device(), &create_info, nullptr, &new_swapchain);
    if (vk_result != VK_SUCCESS) {
        return Failure(ErrorCode::InitializationFailed,
                       "Failed to create the Vulkan swapchain");
    }

    vk_result = vkGetSwapchainImagesKHR(
        impl_->context->Device(), new_swapchain, &image_count, nullptr);
    if (vk_result != VK_SUCCESS || image_count == 0) {
        vkDestroySwapchainKHR(
            impl_->context->Device(), new_swapchain, nullptr);
        return Failure(ErrorCode::InitializationFailed,
                       "Failed to query Vulkan swapchain images");
    }

    std::vector<VkImage> new_images(image_count);
    vk_result = vkGetSwapchainImagesKHR(
        impl_->context->Device(), new_swapchain, &image_count,
        new_images.data());
    if (vk_result != VK_SUCCESS) {
        vkDestroySwapchainKHR(
            impl_->context->Device(), new_swapchain, nullptr);
        return Failure(ErrorCode::InitializationFailed,
                       "Failed to retrieve Vulkan swapchain images");
    }

    std::vector<VkImageView> new_image_views;
    if (Status status = CreateImageViews(new_images, new_image_views);
        !status) {
        vkDestroySwapchainKHR(
            impl_->context->Device(), new_swapchain, nullptr);
        return status;
    }

    std::vector<VkImageView> old_image_views =
        std::move(impl_->image_views);
    const VkSwapchainKHR old_swapchain = impl_->swapchain;

    impl_->swapchain = new_swapchain;
    impl_->images = std::move(new_images);
    impl_->image_views = std::move(new_image_views);
    impl_->image_initialized.assign(impl_->images.size(), false);
    impl_->extent = new_extent;
    impl_->current_image_index = kInvalidImageIndex;

    for (VkImageView image_view : old_image_views) {
        if (image_view != VK_NULL_HANDLE) {
            vkDestroyImageView(
                impl_->context->Device(), image_view, nullptr);
        }
    }
    if (old_swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(
            impl_->context->Device(), old_swapchain, nullptr);
    }

    return {};
}

Status VulkanSwapchain::CreateImageViews(
    const std::vector<VkImage>& images,
    std::vector<VkImageView>& image_views)
{
    image_views.resize(images.size(), VK_NULL_HANDLE);

    for (size_t i = 0; i < images.size(); ++i) {
        VkImageViewCreateInfo create_info {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = images[i],
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
                nullptr, &image_views[i]) != VK_SUCCESS) {
            image_views[i] = VK_NULL_HANDLE;
            for (VkImageView image_view : image_views) {
                if (image_view != VK_NULL_HANDLE) {
                    vkDestroyImageView(
                        impl_->context->Device(), image_view, nullptr);
                }
            }
            image_views.clear();
            return Failure(ErrorCode::InitializationFailed,
                           "Failed to create a Vulkan swapchain image view");
        }
    }

    return {};
}

Status VulkanSwapchain::CreateSyncObjects()
{
    DestroySyncObjects();

    VkSemaphoreCreateInfo semaphore_info {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    };
    VkFenceCreateInfo fence_info {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT,
    };

    if (vkCreateSemaphore(
            impl_->context->Device(), &semaphore_info, nullptr,
            &impl_->image_available) != VK_SUCCESS) {
        DestroySyncObjects();
        return Failure(ErrorCode::InitializationFailed,
                       "Failed to create the image-available semaphore");
    }

    impl_->render_finished.resize(impl_->images.size(), VK_NULL_HANDLE);
    for (VkSemaphore& semaphore : impl_->render_finished) {
        if (vkCreateSemaphore(
                impl_->context->Device(), &semaphore_info, nullptr,
                &semaphore) != VK_SUCCESS) {
            DestroySyncObjects();
            return Failure(ErrorCode::InitializationFailed,
                           "Failed to create a render-finished semaphore");
        }
    }

    if (vkCreateFence(
            impl_->context->Device(), &fence_info, nullptr,
            &impl_->in_flight) != VK_SUCCESS) {
        impl_->in_flight = VK_NULL_HANDLE;
        DestroySyncObjects();
        return Failure(ErrorCode::InitializationFailed,
                       "Failed to create the in-flight fence");
    }

    return {};
}

void VulkanSwapchain::DestroySyncObjects()
{
    if (impl_->in_flight != VK_NULL_HANDLE) {
        vkDestroyFence(impl_->context->Device(), impl_->in_flight, nullptr);
        impl_->in_flight = VK_NULL_HANDLE;
    }
    if (impl_->image_available != VK_NULL_HANDLE) {
        vkDestroySemaphore(
            impl_->context->Device(), impl_->image_available, nullptr);
        impl_->image_available = VK_NULL_HANDLE;
    }
    for (VkSemaphore& semaphore : impl_->render_finished) {
        if (semaphore != VK_NULL_HANDLE) {
            vkDestroySemaphore(
                impl_->context->Device(), semaphore, nullptr);
            semaphore = VK_NULL_HANDLE;
        }
    }
    impl_->render_finished.clear();
}

void VulkanSwapchain::DestroySwapchainResources()
{
    for (VkImageView image_view : impl_->image_views) {
        if (image_view != VK_NULL_HANDLE) {
            vkDestroyImageView(
                impl_->context->Device(), image_view, nullptr);
        }
    }
    impl_->image_views.clear();
    impl_->images.clear();
    impl_->image_initialized.clear();

    if (impl_->swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(
            impl_->context->Device(), impl_->swapchain, nullptr);
        impl_->swapchain = VK_NULL_HANDLE;
    }
    impl_->current_image_index = kInvalidImageIndex;
}

Status VulkanSwapchain::RecreateSwapchain()
{
    if (vkDeviceWaitIdle(impl_->context->Device()) != VK_SUCCESS) {
        return Failure(ErrorCode::RuntimeError,
                       "Failed to wait for the Vulkan device while recreating "
                       "the swapchain");
    }

    if (Status status = CreateSwapchain(); !status) {
        return status;
    }
    if (impl_->images.size() == impl_->render_finished.size()) {
        return {};
    }
    return CreateSyncObjects();
}

Result<SwapchainFrame> VulkanSwapchain::AcquireNextImage()
{
    if (impl_->swapchain == VK_NULL_HANDLE ||
        impl_->in_flight == VK_NULL_HANDLE) {
        return Failure(ErrorCode::NotReady,
                       "The Vulkan swapchain is not initialized");
    }

    for (int attempt = 0; attempt < 2; ++attempt) {
        if (impl_->current_image_index != kInvalidImageIndex) {
            return Failure(ErrorCode::InvalidState,
                           "A swapchain image is already acquired; present it "
                           "before acquiring another image");
        }

        const VkResult wait_result = vkWaitForFences(
            impl_->context->Device(), 1, &impl_->in_flight,
            VK_TRUE, std::numeric_limits<uint64_t>::max());
        if (wait_result != VK_SUCCESS) {
            return Failure(ErrorCode::RuntimeError,
                           "Failed to wait for the in-flight fence");
        }

        uint32_t image_index = kInvalidImageIndex;
        const VkResult acquire_result = vkAcquireNextImageKHR(
            impl_->context->Device(), impl_->swapchain,
            std::numeric_limits<uint64_t>::max(),
            impl_->image_available, VK_NULL_HANDLE, &image_index);

        if (acquire_result == VK_ERROR_OUT_OF_DATE_KHR) {
            if (Status status = RecreateSwapchain(); !status) {
                return std::unexpected(std::move(status.error()));
            }
            continue;
        }
        if (acquire_result != VK_SUCCESS &&
            acquire_result != VK_SUBOPTIMAL_KHR) {
            return Failure(ErrorCode::RuntimeError,
                           "Failed to acquire the next swapchain image");
        }

        if (vkResetFences(
                impl_->context->Device(), 1, &impl_->in_flight) != VK_SUCCESS) {
            return Failure(ErrorCode::RuntimeError,
                           "Failed to reset the in-flight fence");
        }

        impl_->current_image_index = image_index;
        const VkImageLayout initial_layout =
            impl_->image_initialized[image_index]
            ? VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
            : VK_IMAGE_LAYOUT_UNDEFINED;
        const VkImageView image_view = impl_->image_views[image_index];
        const VkImage image = impl_->images[image_index];
        const VkExtent2D extent = impl_->extent;

        impl_->current_render_target = VulkanRenderTarget{
            .image = image,
            .image_view = image_view,
            .format = impl_->format.format,
            .initial_layout = initial_layout,
            .final_layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            .extent = extent,
        };

        return SwapchainFrame{
            .image_index = image_index,
            .extent = GetExtent(),
            .color_format = GetColorFormat(),
            .render_target = RenderTarget{
                .extent = GetExtent(),
                .color_format = GetColorFormat(),
                .native_handle = &impl_->current_render_target,
            },
        };
    }

    return Failure(ErrorCode::OutOfDate,
                   "The Vulkan swapchain remained out of date");
}

Status VulkanSwapchain::WaitIdle()
{
    if (impl_->context->Device() == VK_NULL_HANDLE) {
        return Failure(ErrorCode::NotReady,
                       "The Vulkan device is unavailable");
    }
    if (vkDeviceWaitIdle(impl_->context->Device()) != VK_SUCCESS) {
        return Failure(ErrorCode::RuntimeError,
                       "Failed to wait for the Vulkan device to become idle");
    }
    return {};
}

void VulkanSwapchain::AbandonFrame()
{
    impl_->current_render_target = {};
    impl_->current_image_index = kInvalidImageIndex;
}

Status VulkanSwapchain::Submit(const CommandList& command_list)
{
    if (impl_->current_image_index == kInvalidImageIndex) {
        return Failure(ErrorCode::InvalidState,
                       "No swapchain image has been acquired");
    }

    const auto* vulkan_command_list =
        dynamic_cast<const VulkanCommandList*>(&command_list);
    if (vulkan_command_list == nullptr ||
        vulkan_command_list->NativeHandle() == VK_NULL_HANDLE) {
        return Failure(ErrorCode::InvalidArgument,
                       "Command list is not a Vulkan command list");
    }

    VkSemaphore wait_semaphore = impl_->image_available;
    VkSemaphore signal_semaphore = RenderFinishedSemaphore();
    const VkCommandBuffer command_buffer =
        vulkan_command_list->NativeHandle();

    VkSubmitInfo submit_info{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &wait_semaphore,
        .pWaitDstStageMask = nullptr,
        .commandBufferCount = 1,
        .pCommandBuffers = &command_buffer,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &signal_semaphore,
    };
    const VkPipelineStageFlags wait_stage =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    submit_info.pWaitDstStageMask = &wait_stage;

    VkQueue graphics_queue = VK_NULL_HANDLE;
    for (const VulkanContext::QueueInfo& queue : impl_->context->Queues()) {
        if (queue.family_index == impl_->context->GraphicsQueueFamily()) {
            graphics_queue = queue.queue;
            break;
        }
    }
    if (graphics_queue == VK_NULL_HANDLE) {
        return Failure(ErrorCode::NotReady,
                       "The Vulkan graphics queue is unavailable");
    }

    if (vkQueueSubmit(
            graphics_queue, 1, &submit_info, impl_->in_flight) != VK_SUCCESS) {
        return Failure(ErrorCode::RuntimeError,
                       "Failed to submit the Vulkan command buffer");
    }

    return {};
}

Status VulkanSwapchain::Present()
{
    if (impl_->current_image_index == kInvalidImageIndex ||
        impl_->current_image_index >= impl_->render_finished.size()) {
        return Failure(ErrorCode::InvalidState,
                       "No swapchain image has been acquired");
    }

    if (impl_->present_queue == VK_NULL_HANDLE) {
        return Failure(ErrorCode::NotReady,
                       "The Vulkan present queue is unavailable");
    }

    const uint32_t image_index = impl_->current_image_index;
    const VkSemaphore render_finished = impl_->render_finished[image_index];
    VkPresentInfoKHR present_info {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &render_finished,
        .swapchainCount = 1,
        .pSwapchains = &impl_->swapchain,
        .pImageIndices = &image_index,
    };

    const VkResult present_result =
        vkQueuePresentKHR(impl_->present_queue, &present_info);
    if (present_result == VK_ERROR_OUT_OF_DATE_KHR) {
        impl_->current_render_target = {};
        impl_->current_image_index = kInvalidImageIndex;
        return Failure(ErrorCode::OutOfDate,
                       "The Vulkan swapchain is out of date");
    }
    if (present_result != VK_SUCCESS &&
        present_result != VK_SUBOPTIMAL_KHR) {
        impl_->current_render_target = {};
        impl_->current_image_index = kInvalidImageIndex;
        return Failure(ErrorCode::RuntimeError,
                       "Failed to present the Vulkan swapchain image");
    }

    impl_->image_initialized[image_index] = true;
    impl_->current_render_target = {};
    impl_->current_image_index = kInvalidImageIndex;
    return {};
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

    const VkSurfaceFormatKHR* fallback = nullptr;
    for (const VkSurfaceFormatKHR& format : formats) {
        if (ToColorFormat(format.format) == ColorFormat::UNDEFINED) {
            continue;
        }
        if (fallback == nullptr) {
            fallback = &format;
        }
        if (format.format == VK_FORMAT_B8G8R8A8_UNORM &&
            format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            impl_->format = format;
            return {};
        }
    }

    if (fallback == nullptr) {
        return Failure(
            ErrorCode::Unsupported,
            "The Vulkan surface exposes no supported color formats");
    }

    impl_->format = *fallback;
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
    if (extent.width == 0 || extent.height == 0) {
        return Failure(ErrorCode::InvalidArgument,
                       "Swapchain extent must be non-zero");
    }
    if (impl_->current_image_index != kInvalidImageIndex) {
        return Failure(ErrorCode::InvalidState,
                       "Cannot resize the swapchain while a frame is active");
    }

    impl_->requested_extent = extent;

    VkSurfaceCapabilitiesKHR capabilities{};
    if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
            impl_->context->PhysicalDevice(), impl_->surface,
            &capabilities) != VK_SUCCESS) {
        return Failure(ErrorCode::InitializationFailed,
                       "Failed to query Vulkan surface capabilities");
    }

    const bool fixed_extent =
        capabilities.currentExtent.width !=
        std::numeric_limits<uint32_t>::max();
    const uint32_t target_width = fixed_extent
        ? capabilities.currentExtent.width
        : std::clamp(
            extent.width,
            capabilities.minImageExtent.width,
            capabilities.maxImageExtent.width);
    const uint32_t target_height = fixed_extent
        ? capabilities.currentExtent.height
        : std::clamp(
            extent.height,
            capabilities.minImageExtent.height,
            capabilities.maxImageExtent.height);

    if (target_width == impl_->extent.width &&
        target_height == impl_->extent.height) {
        return {};
    }

    return RecreateSwapchain();
}

Extent VulkanSwapchain::GetExtent() const noexcept
{
    return Extent{
        .width = impl_->extent.width,
        .height = impl_->extent.height,
    };
}

ColorFormat VulkanSwapchain::GetColorFormat() const noexcept
{
    return ToColorFormat(impl_->format.format);
}

VkImageView VulkanSwapchain::CurrentImageView() const noexcept
{
    if (impl_->current_image_index >= impl_->image_views.size()) {
        return VK_NULL_HANDLE;
    }
    return impl_->image_views[impl_->current_image_index];
}

VkImage VulkanSwapchain::CurrentImage() const noexcept
{
    if (impl_->current_image_index >= impl_->images.size()) {
        return VK_NULL_HANDLE;
    }
    return impl_->images[impl_->current_image_index];
}

VkFormat VulkanSwapchain::NativeColorFormat() const noexcept
{
    return impl_->format.format;
}

VkImageLayout VulkanSwapchain::CurrentImageLayout() const noexcept
{
    if (impl_->current_image_index >= impl_->image_initialized.size() ||
        !impl_->image_initialized[impl_->current_image_index]) {
        return VK_IMAGE_LAYOUT_UNDEFINED;
    }
    return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
}

VkSemaphore VulkanSwapchain::ImageAvailableSemaphore() const noexcept
{
    return impl_->image_available;
}

VkSemaphore VulkanSwapchain::RenderFinishedSemaphore() const noexcept
{
    if (impl_->current_image_index >= impl_->render_finished.size()) {
        return VK_NULL_HANDLE;
    }
    return impl_->render_finished[impl_->current_image_index];
}

VkFence VulkanSwapchain::InFlightFence() const noexcept
{
    return impl_->in_flight;
}

uint32_t VulkanSwapchain::CurrentImageIndex() const noexcept
{
    return impl_->current_image_index;
}

} // namespace hotaru
