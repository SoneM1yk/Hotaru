#pragma once

#include <memory>

namespace hotaru {

class RenderSurface 
{
public:
    RenderSurface();
    ~RenderSurface();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}