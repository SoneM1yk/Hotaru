#include "vulkan_driver.h"

#include "vulkan_command_list.h"
#include "vulkan_context.h"
#include "vulkan_pipeline.h"
#include "vulkan_program.h"
#include "vulkan_swapchain.h"

#include <hotaru/config.h>
#include <hotaru/result.h>

#include <memory>
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
VulkanDriver::CreateProgram(std::span<const ShaderBinary> shaders)
{
    return VulkanProgram::Create(impl_->context, shaders);
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

} // namespace hotaru
