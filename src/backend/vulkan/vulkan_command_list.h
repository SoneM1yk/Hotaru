#pragma once

#include "backend/command_list.h"

#include <hotaru/result.h>

#include <memory>
#include <volk.h>

namespace hotaru {

class VulkanContext;
class Frame;
struct RenderPassDesc;
class VulkanSwapchain;

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

    void Begin() override;
    void End() override;
    void BeginRenderPass(
        const Frame& frame, const RenderPassDesc& desc) override;
    void EndRenderPass() override;
    void BindPipeline(const Pipeline* pipeline) override;
    void BindVertexBuffer(Handle<Buffer> buffer) override;
    void Draw(uint32_t vertex_count, uint32_t first_vertex = 0) override;

    VkCommandBuffer* CommandBuffer();
    [[nodiscard]] const Status& Error() const noexcept;

private:
    explicit VulkanCommandList(std::shared_ptr<VulkanContext> context);

    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace hotaru
