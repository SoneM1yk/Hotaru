#pragma once

#include <hotaru/config.h>
#include <hotaru/result.h>

namespace hotaru {

class Program;

enum class ColorFormat
{
    UNDEFINED,
    BGRA8_UNORM,
    RGBA8_UNORM,
    BGRA8_SRGB,
    RGBA8_SRGB,
};

enum class PipelineKind
{
    Graphics,
    Compute,
};

struct PipelineDesc
{
    PipelineKind kind = PipelineKind::Graphics;
    const Program* program = nullptr;
    ColorFormat color_format = ColorFormat::UNDEFINED;
};

class Pipeline
{
public:
    virtual ~Pipeline() = default;

    virtual RenderBackend Backend() const noexcept = 0;
    virtual PipelineKind Kind() const noexcept = 0;
};

} // namespace hotaru
