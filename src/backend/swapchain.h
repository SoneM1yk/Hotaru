#pragma once

#include "hotaru/config.h"
#include <hotaru/result.h>

namespace hotaru {

class CommandList;

class Swapchain
{
public:
    virtual ~Swapchain() = default;

    virtual Status Resize(const Extent& ext) = 0;
    
    virtual uint32_t AcquireNextImage() = 0;
    virtual Status Submit(CommandList* command_list) = 0;
    virtual Status Present(uint32_t index) = 0;
};

} // namespace hotaru
