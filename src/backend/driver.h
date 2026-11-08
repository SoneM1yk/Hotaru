#pragma once

#include <hotaru/config.h>
#include <hotaru/result.h>

#include <memory>

namespace hotaru {

class Frame;
class Swapchain;
class Program;
class CommandList;

class Pipeline;
struct PipelineDesc;

class Buffer;
struct BufferDesc;

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
    CreateProgram() = 0;

    virtual Result<std::unique_ptr<Pipeline>>
    CreatePipeline(const PipelineDesc& desc) = 0;

    virtual Result<std::unique_ptr<CommandList>> 
    CreateCommandList() = 0;

    virtual Result<std::unique_ptr<Buffer>>
    CreateBuffer(const BufferDesc& desc) = 0;

    virtual Status Submit(CommandList* command_list) = 0;
    virtual Status Submit(
        CommandList* command_list, const Frame& frame) = 0;
    virtual Status WaitIdle() = 0;

protected:
    Driver() = default;
};

} // namespace hotaru
