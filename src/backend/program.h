#pragma once 

#include <cstdint>
namespace hotaru {

enum class ShaderStage{
    VERTEX = 0,
    FRAGMENT = 1,
    COMPUTE = 2
};
    
class Program 
{
public:
    virtual void SetShader(ShaderStage stage, 
        const uint8_t* data, uint32_t length) = 0;
};

}