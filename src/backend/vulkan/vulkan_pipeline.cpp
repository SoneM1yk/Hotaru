#include "vulkan_pipeline.h"

#include "vulkan_program.h"
#include "vulkan_context.h"

#include <utility>

#include <volk.h>

namespace hotaru {

namespace {

VkFormat ToVkFormat(ColorFormat format) noexcept
{
    switch (format) {
        case ColorFormat::BGRA8_UNORM:
            return VK_FORMAT_B8G8R8A8_UNORM;
        case ColorFormat::RGBA8_UNORM:
            return VK_FORMAT_R8G8B8A8_UNORM;
        case ColorFormat::BGRA8_SRGB:
            return VK_FORMAT_B8G8R8A8_SRGB;
        case ColorFormat::RGBA8_SRGB:
            return VK_FORMAT_R8G8B8A8_SRGB;
        case ColorFormat::UNDEFINED:
            return VK_FORMAT_UNDEFINED;
    }

    return VK_FORMAT_UNDEFINED;
}

} // namespace

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
    std::unique_ptr<VulkanPipeline> pipeline{
        new VulkanPipeline(context, desc.kind)};

    if (desc.program == nullptr) {
        return Failure(ErrorCode::InvalidArgument,
                       "A pipeline requires a program");
    }
    if (desc.kind != PipelineKind::Graphics) {
        return Failure(ErrorCode::Unsupported,
                       "Only graphics pipelines are supported");
    }
    if (desc.color_format == ColorFormat::UNDEFINED) {
        return Failure(ErrorCode::InvalidArgument,
                       "A graphics pipeline requires a color format");
    }

    const auto* program = dynamic_cast<const VulkanProgram*>(desc.program);
    if (program == nullptr) {
        return Failure(ErrorCode::InvalidArgument,
                       "Program belongs to a different backend");
    }

    const VkShaderModule vertex_shader =
        program->Shader(ShaderStage::VERTEX);
    const VkShaderModule fragment_shader =
        program->Shader(ShaderStage::FRAGMENT);
    if (vertex_shader == VK_NULL_HANDLE ||
        fragment_shader == VK_NULL_HANDLE) {
        return Failure(
            ErrorCode::InvalidArgument,
            "A graphics pipeline requires vertex and fragment shaders");
    }

    // Vertex Input State
    VkPipelineVertexInputStateCreateInfo vertex_input_info {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 0,
        .pVertexBindingDescriptions = nullptr,
        .vertexAttributeDescriptionCount = 0,
        .pVertexAttributeDescriptions = nullptr
    };

    // Input Assembly State
    VkPipelineInputAssemblyStateCreateInfo input_assembly_info {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE
    };

    // Viewport State
    VkPipelineViewportStateCreateInfo viewport_info {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount = 1,
    };

    // Rasterization State
    VkPipelineRasterizationStateCreateInfo rasterization_info {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .depthClampEnable = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_NONE,
        .frontFace = VK_FRONT_FACE_CLOCKWISE,
        .depthBiasEnable = VK_FALSE,
        .lineWidth = 1.0f,
    };

    // Multisample
    VkPipelineMultisampleStateCreateInfo multisample_info {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
        .sampleShadingEnable = VK_FALSE,
        .minSampleShading = 1.f,
        .pSampleMask = nullptr,
        .alphaToCoverageEnable = VK_FALSE,
        .alphaToOneEnable = VK_FALSE
    };

    // Color Blend
    VkPipelineColorBlendAttachmentState color_blend_attachment {
        .blendEnable = VK_FALSE,
        .colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT |
            VK_COLOR_COMPONENT_G_BIT |
            VK_COLOR_COMPONENT_B_BIT |
            VK_COLOR_COMPONENT_A_BIT,
    };

    VkPipelineColorBlendStateCreateInfo color_blend_info {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .logicOpEnable = VK_FALSE,
        .attachmentCount = 1,
        .pAttachments = &color_blend_attachment,
    };

    // Dynamic State
    const VkDynamicState dynamic_states[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
    };

    VkPipelineDynamicStateCreateInfo dynamic_state_info {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = 2,
        .pDynamicStates = dynamic_states,
    };

    // Shader Stages
    const VkPipelineShaderStageCreateInfo shader_stages[] = {
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = vertex_shader,
            .pName = "main",
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = fragment_shader,
            .pName = "main",
        },
    };

    // Layout
    VkPipelineLayoutCreateInfo layout_info {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
    };

    VkResult vk_result = vkCreatePipelineLayout(
        pipeline->impl_->context->Device(), &layout_info, nullptr,
        &pipeline->impl_->layout);
    if (vk_result != VK_SUCCESS) {
        pipeline->impl_->layout = VK_NULL_HANDLE;
        return Failure(ErrorCode::InitializationFailed,
                       "Failed to create Vulkan pipeline layout");
    }

    // Dynamic Rendering
    const VkFormat color_format = ToVkFormat(desc.color_format);
    VkPipelineRenderingCreateInfo rendering_info {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &color_format,
        .depthAttachmentFormat = VK_FORMAT_UNDEFINED,
        .stencilAttachmentFormat = VK_FORMAT_UNDEFINED,
    };

    VkGraphicsPipelineCreateInfo pipeline_info {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = &rendering_info,
        .stageCount = 2,
        .pStages = shader_stages,
        .pVertexInputState = &vertex_input_info,
        .pInputAssemblyState = &input_assembly_info,
        .pViewportState = &viewport_info,
        .pRasterizationState = &rasterization_info,
        .pMultisampleState = &multisample_info,
        .pDepthStencilState = nullptr,
        .pColorBlendState = &color_blend_info,
        .pDynamicState = &dynamic_state_info,
        .layout = pipeline->impl_->layout,
        .renderPass = VK_NULL_HANDLE,
        .subpass = 0,
    };

    vk_result = vkCreateGraphicsPipelines(
        pipeline->impl_->context->Device(), VK_NULL_HANDLE, 1, &pipeline_info,
        nullptr, &pipeline->impl_->pipeline);
    if (vk_result != VK_SUCCESS) {
        pipeline->impl_->pipeline = VK_NULL_HANDLE;
        vkDestroyPipelineLayout(
            pipeline->impl_->context->Device(), pipeline->impl_->layout,
            nullptr);
        pipeline->impl_->layout = VK_NULL_HANDLE;
        return Failure(ErrorCode::InitializationFailed,
                       "Failed to create Vulkan graphics pipeline");
    }

    return pipeline;
}

RenderBackend VulkanPipeline::Backend() const noexcept
{
    return RenderBackend::VULKAN;
}

PipelineKind VulkanPipeline::Kind() const noexcept
{
    return impl_->kind;
}

VkPipeline VulkanPipeline::NativeHandle() const noexcept
{
    return impl_->pipeline;
}

} // namespace hotaru
