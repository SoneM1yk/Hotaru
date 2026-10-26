#pragma once

#include <hotaru/config.h>
#include <hotaru/result.h>

namespace hotaru {

class Camera;

class View 
{
public:
    virtual ~View() = default;

    virtual Camera* GetCamera() const = 0;
    virtual Status Resize(const Extent& ext) = 0;
};

} // namespace hotaru
