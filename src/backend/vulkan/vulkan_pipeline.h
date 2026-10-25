#include "backend/pipeline.h"

#include <memory>

namespace hotaru {

class Program;

class VulkanPipeline: public Pipeline
{
public:
    VulkanPipeline();
    virtual ~VulkanPipeline();

    void SetProgram(Program* program);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}