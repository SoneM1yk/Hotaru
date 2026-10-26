#pragma once

#include "command_list.h"
#include "pipeline.h"
#include "program.h"
#include "swapchain.h"

#include "hotaru/config.h"
#include <hotaru/result.h>

#include <memory>
#include <span>

namespace hotaru {

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

    static Result<std::unique_ptr<Driver>> Create(const DriverConfig& config);

    virtual Result<std::unique_ptr<Swapchain>>
    CreateSwapchain(const NativeSurfaceDesc& desc) = 0;

    virtual Result<std::unique_ptr<Program>>
    CreateProgram(std::span<const ShaderBinary> shaders) = 0;

    virtual Result<std::unique_ptr<Pipeline>>
    CreatePipeline(const PipelineDesc& desc) = 0;

    virtual Result<std::unique_ptr<CommandList>> CreateCommandList() = 0;

protected:
    Driver() = default;
};

} // namespace hotaru
