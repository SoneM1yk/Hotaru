#pragma once

namespace hotaru {

class Scene;

class Renderer 
{
public:
    bool BeginFrame();
    void Render(Scene* scene);
    void EndFrame();

};

}