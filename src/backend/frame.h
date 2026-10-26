#pragma once

#include "hotaru/config.h"

#include <cstdint>

namespace hotaru {

enum class ColorFormat
{
    UNDEFINED,
    BGRA8_UNORM,
    RGBA8_UNORM,
    BGRA8_SRGB,
    RGBA8_SRGB,
};

struct FrameDesc
{
    Extent extent{};
    ColorFormat color_format = ColorFormat::UNDEFINED;
};

struct ClearColor
{
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    float a = 1.0f;
};

struct RenderPassDesc
{
    bool clear = true;
    ClearColor clear_color{};
};

class Frame
{
public:
    virtual ~Frame() = default;

    virtual RenderBackend Backend() const noexcept = 0;
    virtual const FrameDesc& Desc() const noexcept = 0;
    virtual uint32_t ImageIndex() const noexcept = 0;
};

} // namespace hotaru
