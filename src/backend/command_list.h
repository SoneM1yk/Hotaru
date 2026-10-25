#pragma once

namespace hotaru {

class Pipeline;

class CommandList 
{
public:
    virtual void Begin() = 0;
    virtual void End() = 0;

    virtual void BindPipeline(Pipeline* pipeline) = 0;
};

}