#include "vulkan_pipeline.h"

#include "vulkan_context.h"

#include <memory>
#include <utility>

namespace hotaru {

struct VulkanPipeline::Impl
{
    Impl(std::shared_ptr<VulkanContext> context, PipelineKind kind)
        : context(std::move(context))
        , kind(kind)
    {}

    std::shared_ptr<VulkanContext> context;
    PipelineKind kind;
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkPipelineLayout layout = VK_NULL_HANDLE;
};

VulkanPipeline::VulkanPipeline(
    std::shared_ptr<VulkanContext> context,
    PipelineKind kind)
    : impl_(std::make_unique<Impl>(std::move(context), kind))
{}

VulkanPipeline::~VulkanPipeline()
{
    if (impl_->pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(impl_->context->Device(), impl_->pipeline, nullptr);
    }
    if (impl_->layout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(
            impl_->context->Device(), impl_->layout, nullptr);
    }
}

Result<std::unique_ptr<Pipeline>> VulkanPipeline::Create(
    std::shared_ptr<VulkanContext> context,
    const PipelineDesc& desc)
{
    if (desc.program == nullptr) {
        return Failure(ErrorCode::InvalidArgument,
                       "A pipeline requires a program");
    }

    return Failure(ErrorCode::Unsupported,
                   "Vulkan graphics pipeline creation is not implemented yet");
}

RenderBackend VulkanPipeline::Backend() const noexcept
{
    return RenderBackend::VULKAN;
}

PipelineKind VulkanPipeline::Kind() const noexcept
{
    return impl_->kind;
}

} // namespace hotaru
