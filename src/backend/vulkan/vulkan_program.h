#pragma once

#include "backend/program.h"

#include <hotaru/result.h>

#include <memory>
#include <span>

#include <volk.h>

namespace hotaru {

class VulkanContext;

class VulkanProgram final : public Program
{
public:
    static Result<std::unique_ptr<Program>> Create(
        std::shared_ptr<VulkanContext> context,
        std::span<const ShaderBinary> shaders);

    ~VulkanProgram() override;

    VulkanProgram(const VulkanProgram&) = delete;
    VulkanProgram& operator=(const VulkanProgram&) = delete;
    VulkanProgram(VulkanProgram&&) = delete;
    VulkanProgram& operator=(VulkanProgram&&) = delete;

    [[nodiscard]] VkShaderModule Shader(ShaderStage stage) const noexcept;

private:
    explicit VulkanProgram(std::shared_ptr<VulkanContext> context);

    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace hotaru
