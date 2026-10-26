#include "vulkan_command_list.h"

#include "backend/pipeline.h"
#include "vulkan_context.h"

#include <hotaru/result.h>

#include <memory>
#include <utility>

namespace hotaru {

struct VulkanCommandList::Impl
{
    explicit Impl(std::shared_ptr<VulkanContext> context)
        : context(std::move(context))
    {}

    std::shared_ptr<VulkanContext> context;
    bool recording = false;
};

VulkanCommandList::VulkanCommandList(std::shared_ptr<VulkanContext> context)
    : impl_(std::make_unique<Impl>(std::move(context)))
{}

VulkanCommandList::~VulkanCommandList() = default;

Result<std::unique_ptr<CommandList>> VulkanCommandList::Create(
    std::shared_ptr<VulkanContext> context)
{
    return Failure(ErrorCode::Unsupported,
                   "Vulkan command list creation is not implemented yet");
}

Status VulkanCommandList::Begin()
{
    return Failure(ErrorCode::Unsupported,
                   "Vulkan command recording is not implemented yet");
}

Status VulkanCommandList::End()
{
    return Failure(ErrorCode::Unsupported,
                   "Vulkan command recording is not implemented yet");
}

Status VulkanCommandList::BindPipeline(const Pipeline& pipeline)
{
    if (pipeline.Backend() != RenderBackend::VULKAN) {
        return Failure(ErrorCode::InvalidArgument,
                       "Pipeline belongs to a different backend");
    }
    return Failure(ErrorCode::Unsupported,
                   "Vulkan pipeline binding is not implemented yet");
}

Status VulkanCommandList::BindVertexBuffer()
{
    return Failure(ErrorCode::Unsupported,
                   "Vulkan vertex buffer binding is not implemented yet");
}

} // namespace hotaru
