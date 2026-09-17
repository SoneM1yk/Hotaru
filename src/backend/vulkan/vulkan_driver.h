#pragma once

#include "backend/driver.h"

#include <memory>

namespace hotaru {

class CommandList;
class Swapchain;

class VulkanDriver final : public Driver
{
public:
    explicit VulkanDriver(const DriverConfig& config);
    ~VulkanDriver() override;

    VulkanDriver(const VulkanDriver&) = delete;
    VulkanDriver& operator=(const VulkanDriver&) = delete;
    VulkanDriver(VulkanDriver&&) = delete;
    VulkanDriver& operator=(VulkanDriver&&) = delete;

    Result Init() override;

    CommandList* CreateCommandList() override;
    Swapchain* CreateSwapchain(const NativeSurfaceDesc& desc) override;

private:
    Result CreateInstance();
    Result SetupDebugMessenger();
    Result PickPhysicalDevice();
    Result CreateLogicalDevice();

    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace hotaru
