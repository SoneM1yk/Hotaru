#include "vulkan_command_list.h"

#include "vulkan_pipeline.h"
#include "vulkan_context.h"
#include "vulkan_swapchain.h"

#include <hotaru/result.h>

#include <optional>
#include <string>
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
    bool in_render_pass = false;
    bool pipeline_bound = false;
    std::optional<Frame> frame;
    VulkanSwapchain* swapchain = nullptr;
    VulkanSwapchain::ImageState image_state{};
    Status error;

    VkCommandPool command_pool = VK_NULL_HANDLE;
    VkCommandBuffer command_buffer = VK_NULL_HANDLE;

    void Fail(ErrorCode code, std::string message)
    {
        if (!error) {
            error = Failure(code, std::move(message));
        }
    }
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
            &alloc_info, &command_list->impl_->command_buffer) != VK_SUCCESS) {
        return Failure(ErrorCode::InitializationFailed, 
            "Failed to allocate command buffer!");
    }

    return command_list;
}

VkCommandBuffer* VulkanCommandList::CommandBuffer()
{
    return &impl_->command_buffer;
}

const Status& VulkanCommandList::Error() const noexcept
{
    return impl_->error;
}

void VulkanCommandList::Begin()
{
    if (impl_->recording) {
        impl_->Fail(ErrorCode::InvalidState,
            "Current is already recording!");
        return;
    }
    impl_->error = {};

    VkCommandBufferBeginInfo begin_info {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = 0,
        .pInheritanceInfo = nullptr,
    };

    if (vkBeginCommandBuffer(impl_->command_buffer, 
            &begin_info) != VK_SUCCESS) {
        impl_->Fail(ErrorCode::RuntimeError,
            "Failed to begin command buffer!");
        return;
    }

    impl_->recording = true;
}

void VulkanCommandList::End()
{
    if (!impl_->recording) {
        impl_->Fail(ErrorCode::InvalidState,
            "Command buffer is not recording!");
        return;
    }
    if (impl_->in_render_pass) {
        impl_->Fail(ErrorCode::InvalidState,
            "A render pass is still active!");
        return;
    }

    if (vkEndCommandBuffer(impl_->command_buffer) != VK_SUCCESS) {
        impl_->Fail(ErrorCode::RuntimeError,
            "Failed to end command buffer!");
        return;
    }
    impl_->recording = false;
}

void VulkanCommandList::BeginRenderPass(
    const Frame& frame, const RenderPassDesc& desc)
{
    if (!impl_->recording) {
        impl_->Fail(ErrorCode::InvalidState,
            "Command buffer is not recording!");
        return;
    }
    if (impl_->in_render_pass) {
        impl_->Fail(ErrorCode::InvalidState,
            "A render pass is already active!");
        return;
    }

    auto* swapchain = dynamic_cast<VulkanSwapchain*>(frame.swapchain);
    if (swapchain == nullptr) {
        impl_->Fail(ErrorCode::InvalidArgument,
            "Frame belongs to a different backend!");
        return;
    }

    auto image_state = swapchain->GetImageState(frame);
    if (!image_state) {
        impl_->Fail(image_state.error().code, image_state.error().message);
        return;
    }

    const VkImageLayout old_layout = image_state->layout;
    VkImageMemoryBarrier barrier {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = old_layout == VK_IMAGE_LAYOUT_UNDEFINED
            ? VkAccessFlags{0}
            : VkAccessFlags{VK_ACCESS_MEMORY_READ_BIT},
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .oldLayout = old_layout,
        .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image_state->image,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
    };
    vkCmdPipelineBarrier(
        impl_->command_buffer,
        old_layout == VK_IMAGE_LAYOUT_UNDEFINED
            ? VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT
            : VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        0, 0, nullptr, 0, nullptr, 1, &barrier);
    if (Status status = swapchain->SetImageLayout(
            frame, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL); !status) {
        impl_->Fail(status.error().code, status.error().message);
        return;
    }

    VkRenderingAttachmentInfo color_attachment {
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = image_state->view,
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .loadOp = desc.clear ? VK_ATTACHMENT_LOAD_OP_CLEAR
                             : VK_ATTACHMENT_LOAD_OP_LOAD,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue = {
            .color = {
                .float32 = {
                    desc.clear_color.r,
                    desc.clear_color.g,
                    desc.clear_color.b,
                    desc.clear_color.a,
                },
            },
        },
    };

    VkRenderingInfo rendering_info {
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .renderArea = {
            .offset = {0, 0},
            .extent = {
                frame.desc.extent.width,
                frame.desc.extent.height,
            },
        },
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &color_attachment,
    };
    vkCmdBeginRendering(impl_->command_buffer, &rendering_info);

    VkViewport viewport {
        .x = 0.0f,
        .y = 0.0f,
        .width = static_cast<float>(frame.desc.extent.width),
        .height = static_cast<float>(frame.desc.extent.height),
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    };
    VkRect2D scissor {
        .offset = {0, 0},
        .extent = {
            frame.desc.extent.width,
            frame.desc.extent.height,
        },
    };
    vkCmdSetViewport(impl_->command_buffer, 0, 1, &viewport);
    vkCmdSetScissor(impl_->command_buffer, 0, 1, &scissor);

    impl_->frame = frame;
    impl_->swapchain = swapchain;
    impl_->image_state = *image_state;
    impl_->in_render_pass = true;
    impl_->pipeline_bound = false;
}

void VulkanCommandList::EndRenderPass()
{
    if (!impl_->in_render_pass || !impl_->frame.has_value()) {
        impl_->Fail(ErrorCode::InvalidState,
            "No render pass is active!");
        return;
    }

    vkCmdEndRendering(impl_->command_buffer);

    VkImageMemoryBarrier barrier {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_MEMORY_READ_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = impl_->image_state.image,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
    };
    vkCmdPipelineBarrier(
        impl_->command_buffer,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
        0, 0, nullptr, 0, nullptr, 1, &barrier);
    if (Status status = impl_->swapchain->SetImageLayout(
            *impl_->frame, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR); !status) {
        impl_->Fail(status.error().code, status.error().message);
        return;
    }

    impl_->frame.reset();
    impl_->swapchain = nullptr;
    impl_->image_state = {};
    impl_->in_render_pass = false;
    impl_->pipeline_bound = false;
}

void VulkanCommandList::BindPipeline(const Pipeline* pipeline)
{
    if (!impl_->recording) {
        impl_->Fail(ErrorCode::InvalidState,
            "Command buffer is not recording!");
        return;
    }
    if (pipeline == nullptr || pipeline->Backend() != RenderBackend::VULKAN) {
        impl_->Fail(ErrorCode::InvalidArgument,
            "Invalid pipeline!");
        return;
    }

    const VulkanPipeline* vk_pipeline =
        dynamic_cast<const VulkanPipeline*>(pipeline);
    if (vk_pipeline == nullptr) {
        impl_->Fail(ErrorCode::InvalidArgument,
            "Pipeline belongs to a different backend!");
        return;
    }
    vkCmdBindPipeline(impl_->command_buffer, 
        VK_PIPELINE_BIND_POINT_GRAPHICS, vk_pipeline->NativeHandle());
    impl_->pipeline_bound = true;
}

void VulkanCommandList::BindVertexBuffer(Handle<Buffer> buffer)
{
    
}

void VulkanCommandList::Draw(uint32_t vertex_count, uint32_t first_vertex)
{
    if (!impl_->in_render_pass) {
        impl_->Fail(ErrorCode::InvalidState,
            "Draw requires an active render pass!");
        return;
    }
    if (!impl_->pipeline_bound) {
        impl_->Fail(ErrorCode::InvalidState,
            "Draw requires a bound pipeline!");
        return;
    }
    vkCmdDraw(impl_->command_buffer, vertex_count, 1, first_vertex, 0);
}

} // namespace hotaru
