#pragma once

#include <memory>

namespace Hotaru {

class VulkanBackend 
{
public:
    void Init();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};


}