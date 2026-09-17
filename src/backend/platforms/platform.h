#pragma once

#include <memory>

namespace Hotaru {

class Driver;
class SwapChain;

struct PlatformConfig 
{

};

class Platform 
{
public:
    static std::unique_ptr<Platform> Create(const PlatformConfig&);
    virtual ~Platform();

    Driver* CreateDriver();

protected:
    Platform();

    std::unique_ptr<Driver> driver_;
};

}