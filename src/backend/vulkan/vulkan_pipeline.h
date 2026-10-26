#pragma once

#include "backend/pipeline.h"

#include <hotaru/result.h>

#include <memory>

#include <volk.h>

namespace hotaru {

class VulkanContext;

class VulkanPipeline final : public Pipeline
{
public:
    static Result<std::unique_ptr<Pipeline>> Create(
        std::shared_ptr<VulkanContext> context,
        const PipelineDesc& desc);

    ~VulkanPipeline() override;

    VulkanPipeline(const VulkanPipeline&) = delete;
    VulkanPipeline& operator=(const VulkanPipeline&) = delete;
    VulkanPipeline(VulkanPipeline&&) = delete;
    VulkanPipeline& operator=(VulkanPipeline&&) = delete;

    RenderBackend Backend() const noexcept override;
    PipelineKind Kind() const noexcept override;

private:
    VulkanPipeline(std::shared_ptr<VulkanContext> context, PipelineKind kind);

    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace hotaru
