#include <hotaru/renderer.h>
#include <hotaru/result.h>

#include <memory>

namespace hotaru {

class Driver;

class RendererImpl : public Renderer 
{
public:
    RendererImpl();
    virtual ~RendererImpl();

    Status Init(Driver* driver);

    virtual bool BeginFrame(View* view) override;
    virtual void Render() override;
    virtual void EndFrame() override;
    
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}