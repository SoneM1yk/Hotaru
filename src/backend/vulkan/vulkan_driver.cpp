#include "vulkan_driver.h"

#include "vulkan_command_list.h"
#include "vulkan_context.h"
#include "vulkan_frame.h"
#include "vulkan_pipeline.h"
#include "vulkan_program.h"
#include "vulkan_swapchain.h"

#include <hotaru/config.h>
#include <hotaru/result.h>

#include <utility>

namespace hotaru {

struct VulkanDriver::Impl
{
    explicit Impl(std::shared_ptr<VulkanContext> context)
        : context(std::move(context))
    {}

    std::shared_ptr<VulkanContext> context;
};

VulkanDriver::VulkanDriver(std::shared_ptr<VulkanContext> context)
    : impl_(std::make_unique<Impl>(std::move(context)))
{}

VulkanDriver::~VulkanDriver() = default;

Result<std::unique_ptr<Swapchain>>
VulkanDriver::CreateSwapchain(const NativeSurfaceDesc& desc)
{
    return VulkanSwapchain::Create(impl_->context, desc);
}

Result<std::unique_ptr<Program>>
VulkanDriver::CreateProgram()
{
    return VulkanProgram::Create(impl_->context);
}

Result<std::unique_ptr<Pipeline>>
VulkanDriver::CreatePipeline(const PipelineDesc& desc)
{
    return VulkanPipeline::Create(impl_->context, desc);
}

Result<std::unique_ptr<CommandList>> VulkanDriver::CreateCommandList()
{
    return VulkanCommandList::Create(impl_->context);
}

Status VulkanDriver::Submit(CommandList* command_list)
{
    return Submit(command_list, nullptr);
}

Status VulkanDriver::Submit(CommandList* command_list, Frame& frame)
{
    return Submit(command_list, &frame);
}

Status VulkanDriver::Submit(CommandList* command_list, Frame* frame)
{
    VulkanCommandList* vk_command_list =
        dynamic_cast<VulkanCommandList*>(command_list);
    if (vk_command_list == nullptr) {
        return Failure(ErrorCode::InvalidArgument,
            "Command list is Invalid!");
    }
    if (const Status& error = vk_command_list->Error(); !error) {
        return Failure(error.error());
    }

    VkSemaphore wait_semaphore = VK_NULL_HANDLE;
    VkSemaphore signal_semaphore = VK_NULL_HANDLE;
    VkFence fence = VK_NULL_HANDLE;
    if (frame != nullptr) {
        auto* vk_frame = dynamic_cast<VulkanFrame*>(frame);
        if (vk_frame == nullptr) {
            return Failure(ErrorCode::InvalidArgument,
                "Frame belongs to a different backend!");
        }
        wait_semaphore = vk_frame->ImageAvailable();
        signal_semaphore = vk_frame->RenderFinished();
        fence = vk_frame->InFlightFence();
    }

    const VkPipelineStageFlags wait_stages =
        { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    VkSubmitInfo submit_info {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = wait_semaphore != VK_NULL_HANDLE ? 1u : 0u,
        .pWaitSemaphores = wait_semaphore != VK_NULL_HANDLE
            ? &wait_semaphore : nullptr,
        .pWaitDstStageMask = wait_semaphore != VK_NULL_HANDLE
            ? &wait_stages : nullptr,
        .commandBufferCount = 1,
        .pCommandBuffers = vk_command_list->CommandBuffer(),

        .signalSemaphoreCount = signal_semaphore != VK_NULL_HANDLE ? 1u : 0u,
        .pSignalSemaphores = signal_semaphore != VK_NULL_HANDLE
            ? &signal_semaphore : nullptr,
    };

    if (vkQueueSubmit(impl_->context->GraphicsQueue(), 1, &submit_info,
            fence) != VK_SUCCESS) {
        return Failure(ErrorCode::RuntimeError,
            "Failed to submit commands!");
    }

    return {};
}

Status VulkanDriver::WaitIdle()
{
    if (vkQueueWaitIdle(impl_->context->GraphicsQueue()) != VK_SUCCESS) {
        return Failure(ErrorCode::RuntimeError,
            "Failed to wait for the graphics queue!");
    }
    return {};
}

} // namespace hotaru
