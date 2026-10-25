#include "backend/program.h"

#include <memory>
#include <volk.h>

namespace hotaru {

class VulkanProgram: public Program
{
public:
    VulkanProgram(VkDevice device);
    virtual ~VulkanProgram();

    void SetShader(ShaderStage stage, 
        const uint8_t* data, uint32_t length) override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}