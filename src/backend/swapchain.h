#pragma once

#include "hotaru/config.h"
#include <hotaru/result.h>

namespace hotaru {

class Frame;

class Swapchain
{
public:
    virtual ~Swapchain() = default;

    virtual Status Resize(const Extent& ext) = 0;
    
    virtual Result<Frame*> AcquireFrame() = 0;
    virtual Status Present(Frame& frame) = 0;
};

} // namespace hotaru
