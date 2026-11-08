#pragma once

#include "backend/program.h"

#include <hotaru/result.h>

#include <memory>
#include <span>
#include <unordered_map>

#include <volk.h>

namespace hotaru {

class VulkanContext;

class VulkanProgram final : public Program
{
public:
    static Result<std::unique_ptr<Program>> Create(
        std::shared_ptr<VulkanContext> context);

    ~VulkanProgram() override;

    VulkanProgram(const VulkanProgram&) = delete;
    VulkanProgram& operator=(const VulkanProgram&) = delete;
    VulkanProgram(VulkanProgram&&) = delete;
    VulkanProgram& operator=(VulkanProgram&&) = delete;

    [[nodiscard]] VkShaderModule Shader(ShaderStage stage) const noexcept;

    virtual Status SetShader(ShaderStage stage, 
        std::span<const uint8_t> source) override;

private:
    explicit VulkanProgram(std::shared_ptr<VulkanContext> context);

    std::shared_ptr<VulkanContext> context_;
    std::unordered_map<ShaderStage, VkShaderModule> shaders_;
};

} // namespace hotaru
