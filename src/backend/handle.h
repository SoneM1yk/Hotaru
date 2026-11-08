#pragma once

#include <cstdint>

namespace hotaru {

class Buffer;

template<typename T>
class Handle
{
public:
    Handle() noexcept = default;

private:
    uint32_t slot_;
    uint8_t generation_;
};

}