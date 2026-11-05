#pragma once

#include "backend/command_list.h"

#include <memory>

#include <volk.h>

namespace hotaru {

class VulkanContext;

class VulkanCommandList final : public CommandList
{
public:
    static Result<std::unique_ptr<CommandList>> Create(
        std::shared_ptr<VulkanContext> context);

    ~VulkanCommandList() override;

    VulkanCommandList(const VulkanCommandList&) = delete;
    VulkanCommandList& operator=(const VulkanCommandList&) = delete;
    VulkanCommandList(VulkanCommandList&&) = delete;
    VulkanCommandList& operator=(VulkanCommandList&&) = delete;

    Status Begin() override;
    Status End() override;
    Status BeginRendering(const RenderTarget& target) override;
    Status EndRendering() override;
    Status BindPipeline(const Pipeline* pipeline) override;
    Status BindVertexBuffer() override;
    Status Draw(
        uint32_t vertex_count,
        uint32_t instance_count = 1,
        uint32_t first_vertex = 0,
        uint32_t first_instance = 0) override;

    [[nodiscard]] VkCommandBuffer NativeHandle() const noexcept;

private:
    explicit VulkanCommandList(std::shared_ptr<VulkanContext> context);

    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace hotaru
