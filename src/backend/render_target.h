#pragma once

#include "pipeline.h"

#include "hotaru/config.h"

namespace hotaru {

struct RenderTarget
{
    Extent extent{};
    ColorFormat color_format = ColorFormat::UNDEFINED;
    const void* native_handle = nullptr;
};

} // namespace hotaru
