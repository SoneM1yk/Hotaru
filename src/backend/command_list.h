#pragma once

#include "pipeline.h"
#include "render_target.h"

#include <hotaru/result.h>

#include <cstdint>

namespace hotaru {

class Pipeline;

class CommandList
{
public:
    virtual ~CommandList() = default;

    virtual Status Begin() = 0;
    virtual Status End() = 0;

    virtual Status BeginRendering(const RenderTarget& target) = 0;
    virtual Status EndRendering() = 0;

    virtual Status BindPipeline(const Pipeline* pipeline) = 0;
    virtual Status BindVertexBuffer() = 0;
    virtual Status Draw(
        uint32_t vertex_count,
        uint32_t instance_count = 1,
        uint32_t first_vertex = 0,
        uint32_t first_instance = 0) = 0;
};

} // namespace hotaru
