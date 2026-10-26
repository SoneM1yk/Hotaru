#pragma once

namespace hotaru {

class View;

class Renderer 
{
public:
    virtual ~Renderer() = default;

    virtual bool BeginFrame(View* view) = 0;
    virtual void Render() = 0;
    virtual void EndFrame() = 0;

};

}
