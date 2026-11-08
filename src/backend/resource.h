#include <span>
#include <cstdint>

namespace hotaru {

enum class BufferUsage
{
    VERTEX,
};

struct BufferDesc
{
    BufferUsage usage;

    std::span<uint8_t> data;
};

class Buffer
{
public:
    virtual ~Buffer();

    BufferUsage Usage();    

};

}