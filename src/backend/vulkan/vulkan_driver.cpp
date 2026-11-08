#include "vulkan_driver.h"

#include "vulkan_command_list.h"
#include "vulkan_context.h"
#include "vulkan_pipeline.h"
#include "vulkan_program.h"
#include "vulkan_swapchain.h"
#include "vulkan_resource.h"
#include "backend/resource.h"

#include <hotaru/config.h>
#include <hotaru/result.h>

#include <utility>

namespace hotaru {

VulkanDriver::VulkanDriver(std::shared_ptr<VulkanContext> context)
    : context_(std::move(context))
{}

VulkanDriver::~VulkanDriver() = default;

Result<std::unique_ptr<Swapchain>>
VulkanDriver::CreateSwapchain(const NativeSurfaceDesc& desc)
{
    return VulkanSwapchain::Create(context_, desc);
}

Result<std::unique_ptr<Program>>
VulkanDriver::CreateProgram()
{
    return VulkanProgram::Create(context_);
}

Result<std::unique_ptr<Pipeline>>
VulkanDriver::CreatePipeline(const PipelineDesc& desc)
{
    return VulkanPipeline::Create(context_, desc);
}

Result<std::unique_ptr<CommandList>> VulkanDriver::CreateCommandList()
{
    return VulkanCommandList::Create(context_);
}

Result<std::unique_ptr<Buffer>> VulkanDriver::CreateBuffer(
    const BufferDesc& desc)
{
    VkBufferCreateInfo buffer_info {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = desc.data.size(),
        .usage = desc.usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };

    std::unique_ptr<VulkanBuffer> 
}

Status VulkanDriver::Submit(CommandList* command_list)
{
    return Submit(command_list, nullptr);
}

Status VulkanDriver::Submit(
    CommandList* command_list, const Frame& frame)
{
    return Submit(command_list, &frame);
}

Status VulkanDriver::Submit(CommandList* command_list, const Frame* frame)
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
        auto* swapchain = dynamic_cast<VulkanSwapchain*>(frame->swapchain);
        if (swapchain == nullptr) {
            return Failure(ErrorCode::InvalidArgument,
                "Frame belongs to a different backend!");
        }

        auto synchronization = swapchain->GetSynchronization(*frame);
        if (!synchronization) {
            return Failure(synchronization.error());
        }
        wait_semaphore = synchronization->image_available;
        signal_semaphore = synchronization->render_finished;
        fence = synchronization->in_flight_fence;
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

    if (vkQueueSubmit(context_->GraphicsQueue(), 1, &submit_info,
            fence) != VK_SUCCESS) {
        return Failure(ErrorCode::RuntimeError,
            "Failed to submit commands!");
    }

    return {};
}

Status VulkanDriver::WaitIdle()
{
    if (vkQueueWaitIdle(context_->GraphicsQueue()) != VK_SUCCESS) {
        return Failure(ErrorCode::RuntimeError,
            "Failed to wait for the graphics queue!");
    }
    return {};
}

} // namespace hotaru
