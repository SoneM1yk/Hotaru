#pragma once

#include "platforms/platform.h"
namespace Hotaru {

class SwapChain;

class Driver 
{
public:
    virtual ~Driver() = default;

    virtual void Init() = 0;
    virtual SwapChain* CreateSwapChain(void* native_handle, void* native_display) = 0;
};

}