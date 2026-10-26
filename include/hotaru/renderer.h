#pragma once

namespace hotaru {

class Scene;

class Renderer 
{
public:
    virtual ~Renderer() = default;

    bool BeginFrame();
    void Render(Scene* scene);
    void EndFrame();

};

}
