#pragma once

#include "backend/driver.h"

#include <memory>

namespace hotaru {

class VulkanContext;
class Frame;

class VulkanDriver final : public Driver
{
public:
    explicit VulkanDriver(std::shared_ptr<VulkanContext> context);
    ~VulkanDriver() override;

    VulkanDriver(const VulkanDriver&) = delete;
    VulkanDriver& operator=(const VulkanDriver&) = delete;
    VulkanDriver(VulkanDriver&&) = delete;
    VulkanDriver& operator=(VulkanDriver&&) = delete;

    Result<std::unique_ptr<Swapchain>>
    CreateSwapchain(const NativeSurfaceDesc& desc) override;

    Result<std::unique_ptr<Program>>
    CreateProgram() override;

    Result<std::unique_ptr<Pipeline>>
    CreatePipeline(const PipelineDesc& desc) override;

    Result<std::unique_ptr<CommandList>> 
    CreateCommandList() override;

    Status Submit(CommandList* command_list) override;
    Status Submit(CommandList* command_list, Frame& frame) override;
    Status WaitIdle() override;

private:
    Status Submit(CommandList* command_list, Frame* frame);

    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace hotaru
