#pragma once

#include <hotaru/renderer.h>

#include "backend/pipeline.h"

#include <hotaru/config.h>
#include <hotaru/result.h>

#include <memory>

namespace hotaru {

class Driver;

class RendererImpl : public Renderer 
{
public:
    RendererImpl(Driver* driver, ColorFormat color_format);
    virtual ~RendererImpl();

    virtual bool BeginFrame(View* view) override;
    virtual void Render() override;
    virtual void EndFrame() override;
    
private:
    bool EnsurePipeline(ColorFormat color_format);

    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}
