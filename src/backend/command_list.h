#pragma once

#include <hotaru/result.h>

namespace hotaru {

class Pipeline;

class CommandList
{
public:
    virtual ~CommandList() = default;

    virtual Status Begin() = 0;
    virtual Status End() = 0;

    virtual Status BindPipeline(const Pipeline& pipeline) = 0;
    virtual Status BindVertexBuffer() = 0;
};

} // namespace hotaru
