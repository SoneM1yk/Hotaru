#pragma once

#include "hotaru/config.h"
#include <hotaru/result.h>

#include <cstdint>
#include <memory>
#include <span>

#include <volk.h>

namespace hotaru {

class VulkanContext final
{
public:
    struct QueueInfo
    {
        VkQueue queue = VK_NULL_HANDLE;
        uint32_t family_index = VK_QUEUE_FAMILY_IGNORED;
        VkQueueFlags flags = 0;
    };

    static Result<std::shared_ptr<VulkanContext>>
    Create(SurfaceType surface_type);

    ~VulkanContext();

    VulkanContext(const VulkanContext&) = delete;
    VulkanContext& operator=(const VulkanContext&) = delete;

    [[nodiscard]] VkInstance Instance() const noexcept;
    [[nodiscard]] VkPhysicalDevice PhysicalDevice() const noexcept;
    [[nodiscard]] VkDevice Device() const noexcept;
    [[nodiscard]] uint32_t GraphicsQueueFamily() const noexcept;
    [[nodiscard]] std::span<const QueueInfo> Queues() const noexcept;
    [[nodiscard]] SurfaceType RequestedSurfaceType() const noexcept;

private:
    explicit VulkanContext(SurfaceType surface_type);

    Status Init();
    Status CreateInstance();
    Status SetupDebugMessenger();
    Status PickPhysicalDevice();
    Status CreateLogicalDevice();

    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace hotaru
