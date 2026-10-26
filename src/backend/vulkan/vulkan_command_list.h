#pragma once

#include "backend/command_list.h"

#include <memory>

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
    Status BindPipeline(const Pipeline& pipeline) override;
    Status BindVertexBuffer() override;

private:
    explicit VulkanCommandList(std::shared_ptr<VulkanContext> context);

    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace hotaru
