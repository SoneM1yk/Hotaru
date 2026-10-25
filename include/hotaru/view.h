#pragma once

#include <hotaru/config.h>

namespace hotaru {

class Camera;

class View 
{
public:
    virtual Camera* GetCamera() const = 0;
    virtual void Resize(const Extent& ext) = 0;
};

}