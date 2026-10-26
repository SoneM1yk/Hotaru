#pragma once

#include <cstdint>
#include <span>

#include <hotaru/result.h>

namespace hotaru {

enum class ShaderStage
{
    VERTEX = 0,
    FRAGMENT = 1,
    COMPUTE = 2
};

class Program
{
public:
    virtual ~Program() = default;

    virtual Status SetShader(ShaderStage stage, 
        std::span<const uint8_t> source) = 0;
};

} // namespace hotaru
