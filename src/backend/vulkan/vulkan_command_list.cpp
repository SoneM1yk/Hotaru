#include "vulkan_command_list.h"

#include "backend/swapchain.h"
#include "vulkan_swapchain.h"
#include "vulkan_pipeline.h"
#include "vulkan_context.h"

#include <hotaru/result.h>

#include <utility>

namespace hotaru {

struct VulkanCommandList::Impl
{
    explicit Impl(std::shared_ptr<VulkanContext> context)
        : context(std::move(context))
    {}

    ~Impl() {
        vkDestroyCommandPool(context->Device(), command_pool, nullptr);
    }

    std::shared_ptr<VulkanContext> context;
    bool recording = false;

    VkCommandPool command_pool = VK_NULL_HANDLE;
    VkCommandBuffer command_buffer = VK_NULL_HANDLE;
};

VulkanCommandList::VulkanCommandList(std::shared_ptr<VulkanContext> context)
    : impl_(std::make_unique<Impl>(std::move(context)))
{}

VulkanCommandList::~VulkanCommandList() = default;

Result<std::unique_ptr<CommandList>> VulkanCommandList::Create(
    std::shared_ptr<VulkanContext> context)
{
    std::unique_ptr<VulkanCommandList> command_list{
        new VulkanCommandList(context)};
    
    VkCommandPoolCreateInfo create_info {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = 0,
        .queueFamilyIndex = context->GraphicsQueueFamily(),
    };

    if (vkCreateCommandPool(context->Device(), &create_info, 
            nullptr, &command_list->impl_->command_pool) != VK_SUCCESS) {
        return Failure(ErrorCode::InitializationFailed, 
            "Failed to create vulkan command pool!");
    }

    VkCommandBufferAllocateInfo alloc_info {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = command_list->impl_->command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };

    if (vkAllocateCommandBuffers(command_list->impl_->context->Device(), 
            nullptr, &command_list->impl_->command_buffer) != VK_SUCCESS) {
        return Failure(ErrorCode::InitializationFailed, 
            "Failed to allocate command buffer!");
    }

    return command_list;
}

Status VulkanCommandList::Begin(Swapchain* swapchain)
{
    if (impl_->recording) {
        return Failure(ErrorCode::InvalidState, 
            "Current is already recording!");
    }

    auto vk_swapchain = dynamic_cast<VulkanSwapchain*>(swapchain);
    if (vk_swapchain == nullptr) {
        return Failure(ErrorCode::InvalidArgument, 
            "Swapchain is nullptr!");
    }

    VkCommandBufferBeginInfo begin_info {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT,
        .pInheritanceInfo = nullptr,
    };

    if (vkBeginCommandBuffer(impl_->command_buffer, 
            &begin_info) != VK_SUCCESS) {
        return Failure(ErrorCode::RuntimeError, 
            "Failed to begin command buffer!");
    }

    impl_->recording = true;
    return {};
}

Status VulkanCommandList::End()
{
    if (vkEndCommandBuffer(impl_->command_buffer) != VK_SUCCESS) {
        return Failure(ErrorCode::RuntimeError, 
            "Failed to end command buffer!");
    }
    impl_->recording = false;
    return {};
}

Status VulkanCommandList::BindPipeline(const Pipeline* pipeline)
{
    if (pipeline == nullptr || pipeline->Backend() != RenderBackend::VULKAN) {
        return Failure(ErrorCode::InvalidArgument,
                       "Invalid pipeline!");
    }

    const VulkanPipeline* vk_pipeline = 
        static_cast<const VulkanPipeline*>(pipeline);
    vkCmdBindPipeline(impl_->command_buffer, 
        VK_PIPELINE_BIND_POINT_GRAPHICS, vk_pipeline->NativeHandle());

    return {};
}

Status VulkanCommandList::BindVertexBuffer()
{
    return Failure(ErrorCode::Unsupported,
                   "Vulkan vertex buffer binding is not implemented yet");
}

} // namespace hotaru
