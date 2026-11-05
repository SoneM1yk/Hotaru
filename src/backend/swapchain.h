#pragma once

#include "pipeline.h"
#include "render_target.h"

#include "hotaru/config.h"
#include <hotaru/result.h>

#include <cstdint>

namespace hotaru {

class CommandList;

struct SwapchainFrame
{
    uint32_t image_index = 0;
    Extent extent{};
    ColorFormat color_format = ColorFormat::UNDEFINED;
    RenderTarget render_target{};
};

class Swapchain
{
public:
    virtual ~Swapchain() = default;

    virtual Result<SwapchainFrame> AcquireNextImage() = 0;
    virtual Status Submit(const CommandList& command_list) = 0;
    virtual Status WaitIdle() = 0;
    virtual void AbandonFrame() = 0;
    virtual Status Present() = 0;
    virtual Status Resize(const Extent& ext) = 0;

    [[nodiscard]] virtual Extent GetExtent() const noexcept = 0;
    [[nodiscard]] virtual ColorFormat GetColorFormat() const noexcept = 0;
};

} // namespace hotaru
