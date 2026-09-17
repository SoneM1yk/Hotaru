#pragma once

#include <memory>
#include <vector>
#include "driver.h"

namespace Hotaru {

class SwapChain;

class VulkanDriver : public Driver
{
public:
    VulkanDriver(std::vector<const char*> extensions);
    virtual ~VulkanDriver();

    void Init() override;
    SwapChain* CreateSwapChain(void* native_handle, void* native_display) override;

private:
    void CreateInstance();
    void PickPhysicalDevice();
    void CreateLogicalDevice();

    struct Impl;
    std::unique_ptr<Impl> impl_;
};


}