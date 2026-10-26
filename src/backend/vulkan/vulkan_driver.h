#pragma once

#include "backend/driver.h"

#include <memory>

namespace hotaru {

class VulkanContext;

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
    CreateProgram(std::span<const ShaderBinary> shaders) override;

    Result<std::unique_ptr<Pipeline>>
    CreatePipeline(const PipelineDesc& desc) override;

    Result<std::unique_ptr<CommandList>> CreateCommandList() override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace hotaru
