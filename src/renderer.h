#include <hotaru/renderer.h>
#include <hotaru/result.h>

#include "backend/frame.h"

#include <memory>
#include <optional>

namespace hotaru {

class Driver;
class CommandList;
class Pipeline;
class Program;

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
    Driver* driver_ = nullptr;
    std::unique_ptr<Pipeline> pipeline_;
    std::unique_ptr<CommandList> command_list_;
    std::unique_ptr<Program> program_;

    Swapchain* swapchain_ = nullptr;
    std::optional<Frame> frame_;
};

}
