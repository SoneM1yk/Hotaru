#pragma once

#include <hotaru/config.h>
#include <hotaru/view.h>
#include <hotaru/camera.h>
#include <memory>

namespace hotaru {

class Swapchain;

class ViewImpl : public View
{
public:
    ViewImpl(Swapchain* swapchain);
    virtual ~ViewImpl();

    Camera* GetCamera() const override;
    void Resize(const Extent& ext) override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}