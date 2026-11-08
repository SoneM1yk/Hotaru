#include "vulkan_program.h"

#include "backend/program.h"
#include "vulkan_context.h"

#include <cstdint>
#include <utility>
#include <unordered_map>

#include <spdlog/spdlog.h>

namespace hotaru {

VulkanProgram::VulkanProgram(std::shared_ptr<VulkanContext> context)
    : context_(std::move(context))
{}

VulkanProgram::~VulkanProgram()
{
    for (const auto& [_, shader_module] : shaders_) {
        vkDestroyShaderModule(context_->Device(), shader_module, nullptr);
    }
}

Result<std::unique_ptr<Program>> VulkanProgram::Create(
    std::shared_ptr<VulkanContext> context)
{
    std::unique_ptr<VulkanProgram> program(
        new VulkanProgram(std::move(context)));
    return program;
}

VkShaderModule VulkanProgram::Shader(ShaderStage stage) const noexcept
{
    const auto shader = shaders_.find(stage);
    return shader != shaders_.end() ? shader->second : VK_NULL_HANDLE;
}

Status VulkanProgram::SetShader(ShaderStage stage, 
    std::span<const uint8_t> source) 
{
    if (source.empty() || source.size() % sizeof(uint32_t) != 0) {
        return Failure(
            ErrorCode::InvalidArgument,
            "Shader SPIR-V code must be non-empty and 4-byte aligned");
    }

    VkShaderModuleCreateInfo create_info {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = source.size(),
        .pCode = reinterpret_cast<const uint32_t*>(source.data()),
    };

    VkShaderModule shader_module = VK_NULL_HANDLE;
    const VkResult vk_result = vkCreateShaderModule(
        context_->Device(), &create_info, nullptr,
        &shader_module);
    if (vk_result != VK_SUCCESS) {
        return Failure(ErrorCode::InitializationFailed,
                        "Failed to create a Vulkan shader module");
    }

    shaders_.emplace(stage, shader_module);

    auto to_vkstage = [] (ShaderStage stage) {
        switch (stage) {
            case ShaderStage::VERTEX:
                return VK_SHADER_STAGE_VERTEX_BIT;
            case ShaderStage::FRAGMENT:
                return VK_SHADER_STAGE_FRAGMENT_BIT;
            case ShaderStage::COMPUTE:
                return VK_SHADER_STAGE_COMPUTE_BIT;
        }
        return VK_SHADER_STAGE_ALL;
    };

    VkPipelineShaderStageCreateInfo staget_create_info {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = to_vkstage(stage),
        .module = shader_module,
        .pName = "main",
    };


    return {};
}

} // namespace hotaru
