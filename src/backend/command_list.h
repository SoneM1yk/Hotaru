#pragma once

#include <cstdint>

namespace hotaru {

class Pipeline;
class Frame;
struct RenderPassDesc;

class CommandList
{
public:
    virtual ~CommandList() = default;

    virtual void Begin() = 0;
    virtual void End() = 0;

    virtual void BeginRenderPass(
        Frame* frame, const RenderPassDesc& desc) = 0;
    virtual void EndRenderPass() = 0;

    virtual void BindPipeline(const Pipeline* pipeline) = 0;
    virtual void BindVertexBuffer() = 0;
    virtual void Draw(uint32_t vertex_count, uint32_t first_vertex = 0) = 0;
};

} // namespace hotaru
