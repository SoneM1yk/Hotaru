#pragma once

namespace hotaru {

class Entity;

class Scene 
{
public:
    virtual ~Scene() = default;

    Entity* AddEntity();


};

}
