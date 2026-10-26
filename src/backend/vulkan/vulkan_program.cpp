#include "vulkan_program.h"

#include "backend/program.h"
#include "vulkan_context.h"

#include <cstdint>
#include <memory>
#include <span>
#include <utility>
#include <unordered_map>

#include <spdlog/spdlog.h>

namespace hotaru {

struct VulkanProgram::Impl
{
    explicit Impl(std::shared_ptr<VulkanContext> context)
        : context(std::move(context))
    {}

    ~Impl()
    {
        for (const auto& [_, shader_module] : shaders) {
            vkDestroyShaderModule(context->Device(), shader_module, nullptr);
        }
    }

    std::shared_ptr<VulkanContext> context;
    std::unordered_map<ShaderStage, VkShaderModule> shaders;
};

VulkanProgram::VulkanProgram(std::shared_ptr<VulkanContext> context)
    : impl_(std::make_unique<Impl>(std::move(context)))
{}

VulkanProgram::~VulkanProgram() = default;

Result<std::unique_ptr<Program>> VulkanProgram::Create(
    std::shared_ptr<VulkanContext> context,
    std::span<const ShaderBinary> shaders)
{
    if (shaders.empty()) {
        return Failure(ErrorCode::InvalidArgument,
                       "A program requires at least one shader");
    }

    std::unique_ptr<VulkanProgram> program(
        new VulkanProgram(std::move(context)));

    for (const ShaderBinary& shader : shaders) {
        if (program->impl_->shaders.contains(shader.stage)) {
            return Failure(ErrorCode::InvalidArgument,
                           "A shader stage may only be specified once");
        }
        if (shader.code.empty() || shader.code.size() % sizeof(uint32_t) != 0) {
            return Failure(
                ErrorCode::InvalidArgument,
                "Shader SPIR-V code must be non-empty and 4-byte aligned");
        }

        VkShaderModuleCreateInfo create_info {
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .codeSize = shader.code.size(),
            .pCode = reinterpret_cast<const uint32_t*>(shader.code.data()),
        };

        VkShaderModule shader_module = VK_NULL_HANDLE;
        const VkResult vk_result = vkCreateShaderModule(
            program->impl_->context->Device(), &create_info, nullptr,
            &shader_module);
        if (vk_result != VK_SUCCESS) {
            return Failure(ErrorCode::InitializationFailed,
                           "Failed to create a Vulkan shader module");
        }

        program->impl_->shaders.emplace(shader.stage, shader_module);
    }

    std::unique_ptr<Program> result(std::move(program));
    return result;
}

VkShaderModule VulkanProgram::Shader(ShaderStage stage) const noexcept
{
    const auto shader = impl_->shaders.find(stage);
    return shader != impl_->shaders.end() ? shader->second : VK_NULL_HANDLE;
}

} // namespace hotaru
