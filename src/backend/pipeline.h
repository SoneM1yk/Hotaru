#pragma once

#include "hotaru/config.h"

namespace hotaru {

class Program;

enum class PipelineKind
{
    Graphics,
    Compute,
};

struct PipelineDesc
{
    PipelineKind kind = PipelineKind::Graphics;
    const Program* program = nullptr;
};

class Pipeline
{
public:
    virtual ~Pipeline() = default;

    virtual RenderBackend Backend() const noexcept = 0;
    virtual PipelineKind Kind() const noexcept = 0;
};

} // namespace hotaru
