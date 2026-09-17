#pragma once

#include "hotaru/config.h"
#include "result.h"

#include <memory>

namespace hotaru {

class Swapchain;
class CommandList;

struct DriverConfig
{
    RenderBackend backend = RenderBackend::VULKAN;
    SurfaceType surface_type = SurfaceType::NONE;
};

class Driver 
{
public:
    Driver(const Driver&) = delete;
    Driver& operator=(const Driver&) = delete;
    Driver(Driver&&) = delete;
    Driver& operator=(Driver&&) = delete;

    virtual ~Driver() = default;

    static std::unique_ptr<Driver> Create(const DriverConfig& config);

    virtual Swapchain* CreateSwapchain(const NativeSurfaceDesc& desc) = 0;
    virtual CommandList* CreateCommandList() = 0;

protected:
    Driver() = default;
    virtual Result Init() = 0;
};

} // namespace hotaru
