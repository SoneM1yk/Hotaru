#pragma once

namespace hotaru {

class Driver;

struct PlatformConfig 
{

};

class Platform 
{
public:
    struct SwapChain {
    protected:
        ~SwapChain() = default;
    };

    Platform(const PlatformConfig& config);
    virtual ~Platform();

    Driver* CreateDriver();
    
};

}