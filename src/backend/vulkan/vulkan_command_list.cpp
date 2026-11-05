#include "vulkan_command_list.h"

#include "backend/pipeline.h"
#include "vulkan_context.h"
#include "vulkan_pipeline.h"
#include "vulkan_render_target.h"

#include <hotaru/result.h>

#include <cstring>
#include <cstdint>
#include <utility>

namespace hotaru {

struct VulkanCommandList::Impl
{
    explicit Impl(std::shared_ptr<VulkanContext> context)
        : context(std::move(context))
    {}

    ~Impl()
    {
        if (command_pool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(context->Device(), command_pool, nullptr);
        }
    }

    std::shared_ptr<VulkanContext> context;
    bool recording = false;
    bool rendering = false;
    bool pipeline_bound = false;
    VulkanRenderTarget target{};

    VkCommandPool command_pool = VK_NULL_HANDLE;
    VkCommandBuffer command_buffer = VK_NULL_HANDLE;
    VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
};

namespace {

void ImageBarrier(
    const VkCommandBuffer command_buffer,
    const VkImage image,
    const VkImageLayout old_layout,
    const VkImageLayout new_layout,
    const VkAccessFlags src_access,
    const VkAccessFlags dst_access,
    const VkPipelineStageFlags src_stage,
    const VkPipelineStageFlags dst_stage)
{
    VkImageMemoryBarrier barrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = src_access,
        .dstAccessMask = dst_access,
        .oldLayout = old_layout,
        .newLayout = new_layout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
    };

    vkCmdPipelineBarrier(
        command_buffer, src_stage, dst_stage, 0, 0, nullptr, 0, nullptr,
        1, &barrier);
}

} // namespace

VulkanCommandList::VulkanCommandList(std::shared_ptr<VulkanContext> context)
    : impl_(std::make_unique<Impl>(std::move(context)))
{}

VulkanCommandList::~VulkanCommandList()
{}

Result<std::unique_ptr<CommandList>> VulkanCommandList::Create(
    std::shared_ptr<VulkanContext> context)
{
    std::unique_ptr<VulkanCommandList> command_list{
        new VulkanCommandList(context)};

    VkCommandPoolCreateInfo create_info{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = context->GraphicsQueueFamily(),
    };

    if (vkCreateCommandPool(context->Device(), &create_info, nullptr,
            &command_list->impl_->command_pool) != VK_SUCCESS) {
        return Failure(ErrorCode::InitializationFailed,
                       "Failed to create the Vulkan command pool");
    }

    VkCommandBufferAllocateInfo alloc_info{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = command_list->impl_->command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };

    if (vkAllocateCommandBuffers(context->Device(), &alloc_info,
            &command_list->impl_->command_buffer) != VK_SUCCESS) {
        return Failure(ErrorCode::InitializationFailed,
                       "Failed to allocate the Vulkan command buffer");
    }

    return command_list;
}

Status VulkanCommandList::Begin()
{
    if (impl_->recording) {
        return Failure(ErrorCode::InvalidState,
                       "The command list is already recording");
    }

    if (vkResetCommandBuffer(
            impl_->command_buffer, 0) != VK_SUCCESS) {
        return Failure(ErrorCode::RuntimeError,
                       "Failed to reset the Vulkan command buffer");
    }

    VkCommandBufferBeginInfo begin_info{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };

    if (vkBeginCommandBuffer(impl_->command_buffer, &begin_info) != VK_SUCCESS) {
        return Failure(ErrorCode::RuntimeError,
                       "Failed to begin the Vulkan command buffer");
    }

    impl_->recording = true;
    impl_->pipeline_bound = false;
    return {};
}

Status VulkanCommandList::End()
{
    if (!impl_->recording) {
        return Failure(ErrorCode::InvalidState,
                       "The command list is not recording");
    }

    if (impl_->rendering) {
        EndRendering();
    }

    if (vkEndCommandBuffer(impl_->command_buffer) != VK_SUCCESS) {
        return Failure(ErrorCode::RuntimeError,
                       "Failed to end the Vulkan command buffer");
    }

    impl_->recording = false;
    return {};
}

Status VulkanCommandList::BeginRendering(const RenderTarget& target)
{
    if (!impl_->recording) {
        return Failure(ErrorCode::InvalidState,
                       "Begin a command list before beginning rendering");
    }
    if (impl_->rendering) {
        return Failure(ErrorCode::InvalidState,
                       "Rendering has already begun");
    }
    if (target.native_handle == nullptr) {
        return Failure(ErrorCode::InvalidArgument,
                       "Render target has no native handle");
    }

    const auto* vulkan_target =
        static_cast<const VulkanRenderTarget*>(target.native_handle);
    if (vulkan_target->image == VK_NULL_HANDLE ||
        vulkan_target->image_view == VK_NULL_HANDLE ||
        vulkan_target->format == VK_FORMAT_UNDEFINED) {
        return Failure(ErrorCode::InvalidArgument,
                       "Render target is incomplete");
    }

    impl_->target = *vulkan_target;

    ImageBarrier(
        impl_->command_buffer,
        impl_->target.image,
        impl_->target.initial_layout,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        0,
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

    VkRenderingAttachmentInfo color_attachment{
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = impl_->target.image_view,
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue = {.color = {.float32 = {0.05f, 0.05f, 0.08f, 1.0f}}},
    };

    VkRenderingInfo rendering_info{
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .renderArea = {
            .offset = {0, 0},
            .extent = impl_->target.extent,
        },
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &color_attachment,
    };

    vkCmdBeginRendering(impl_->command_buffer, &rendering_info);

    VkViewport viewport{
        .x = 0.0f,
        .y = 0.0f,
        .width = static_cast<float>(impl_->target.extent.width),
        .height = static_cast<float>(impl_->target.extent.height),
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    };
    vkCmdSetViewport(impl_->command_buffer, 0, 1, &viewport);

    VkRect2D scissor{
        .offset = {0, 0},
        .extent = impl_->target.extent,
    };
    vkCmdSetScissor(impl_->command_buffer, 0, 1, &scissor);

    impl_->rendering = true;
    return {};
}

Status VulkanCommandList::EndRendering()
{
    if (!impl_->rendering) {
        return Failure(ErrorCode::InvalidState,
                       "Rendering has not begun");
    }

    vkCmdEndRendering(impl_->command_buffer);

    ImageBarrier(
        impl_->command_buffer,
        impl_->target.image,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        impl_->target.final_layout,
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        0,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);

    impl_->rendering = false;
    return {};
}

Status VulkanCommandList::BindPipeline(const Pipeline* pipeline)
{
    if (pipeline == nullptr || pipeline->Backend() != RenderBackend::VULKAN) {
        return Failure(ErrorCode::InvalidArgument,
                       "A Vulkan command list requires a Vulkan pipeline");
    }

    const auto* vulkan_pipeline =
        dynamic_cast<const VulkanPipeline*>(pipeline);
    if (vulkan_pipeline == nullptr ||
        vulkan_pipeline->NativeHandle() == VK_NULL_HANDLE) {
        return Failure(ErrorCode::InvalidArgument,
                       "Pipeline has no native Vulkan handle");
    }

    vkCmdBindPipeline(
        impl_->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
        vulkan_pipeline->NativeHandle());
    impl_->pipeline_layout = vulkan_pipeline->NativeLayout();
    impl_->pipeline_bound = true;

    return {};
}

Status VulkanCommandList::BindVertexBuffer()
{
    return Failure(ErrorCode::Unsupported,
                   "Vertex buffers are not required by the triangle shader");
}

Status VulkanCommandList::Draw(
    const uint32_t vertex_count,
    const uint32_t instance_count,
    const uint32_t first_vertex,
    const uint32_t first_instance)
{
    if (!impl_->recording || !impl_->rendering) {
        return Failure(ErrorCode::InvalidState,
                       "Draw requires an active rendering pass");
    }
    if (!impl_->pipeline_bound || impl_->pipeline_layout == VK_NULL_HANDLE) {
        return Failure(ErrorCode::InvalidState,
                       "Bind a pipeline before drawing");
    }

    vkCmdDraw(
        impl_->command_buffer, vertex_count, instance_count,
        first_vertex, first_instance);
    return {};
}

VkCommandBuffer VulkanCommandList::NativeHandle() const noexcept
{
    return impl_->command_buffer;
}

} // namespace hotaru
