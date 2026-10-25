#pragma once

#include "result.h"
#include "hotaru/config.h"

namespace hotaru {

class Swapchain
{
public:
    virtual Result Resize(const Extent& ext) = 0;

};

}