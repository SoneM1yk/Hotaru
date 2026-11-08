#pragma once

#include "backend/frame.h"
#include "backend/handle.h"

#include <cstdint>

namespace hotaru {

class Pipeline;
struct RenderPassDesc;

class CommandList
{
public:
    virtual ~CommandList() = default;

    virtual void Begin() = 0;
    virtual void End() = 0;

    virtual void BeginRenderPass(
        const Frame& frame, const RenderPassDesc& desc) = 0;
    virtual void EndRenderPass() = 0;

    virtual void BindPipeline(const Pipeline* pipeline) = 0;
    virtual void BindVertexBuffer(Handle<Buffer> buffer) = 0;
    virtual void Draw(uint32_t vertex_count, uint32_t first_vertex = 0) = 0;
};

} // namespace hotaru
