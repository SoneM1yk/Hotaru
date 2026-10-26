#include <hotaru/renderer.h>

#include <memory>

namespace hotaru {

class RendererImpl : public Renderer 
{
public:
    RendererImpl();
    virtual ~RendererImpl();

    virtual bool BeginFrame(View* view) override;
    virtual void Render() override;
    virtual void EndFrame() override;
    
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}