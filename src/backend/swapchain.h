#pragma once

#include "hotaru/config.h"
#include <hotaru/result.h>

namespace hotaru {

class Swapchain
{
public:
    virtual ~Swapchain() = default;

    virtual Status Resize(const Extent& ext) = 0;
};

} // namespace hotaru
