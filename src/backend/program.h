#pragma once

#include <cstdint>
#include <span>

namespace hotaru {

enum class ShaderStage
{
    VERTEX = 0,
    FRAGMENT = 1,
    COMPUTE = 2
};

struct ShaderBinary
{
    ShaderStage stage = ShaderStage::VERTEX;
    std::span<const uint8_t> code;
};

class Program
{
public:
    virtual ~Program() = default;
};

} // namespace hotaru
