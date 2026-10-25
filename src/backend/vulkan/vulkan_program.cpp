#include "vulkan_program.h"
#include "backend/program.h"

#include <unordered_map>

#include <spdlog/spdlog.h>

namespace hotaru {

struct VulkanProgram::Impl
{
    ~Impl()
    {
        for (const auto& [_, shader_module] : shaders) {
            vkDestroyShaderModule(device, shader_module, nullptr);
        }
    }

    std::unordered_map<ShaderStage, VkShaderModule> shaders; 
    VkDevice device;
};

VulkanProgram::VulkanProgram(VkDevice device):
    impl_(std::make_unique<Impl>())
{
    impl_->device = device;
}

void VulkanProgram::SetShader(ShaderStage stage, 
        const uint8_t* data, uint32_t length)
{
    VkShaderModuleCreateInfo create_info {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = length,
        .pCode = reinterpret_cast<const uint32_t*>(data),
    };
    VkShaderModule shader_module;
    if (vkCreateShaderModule(impl_->device, 
        &create_info, nullptr, &shader_module) != VK_SUCCESS) {
        SPDLOG_ERROR("Create Shader Module Failed!");
        return;
    }

    impl_->shaders[stage] = shader_module;
}



}