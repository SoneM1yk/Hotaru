#pragma once

#include <hotaru/config.h>
#include <hotaru/result.h>
#include <hotaru/view.h>
#include <hotaru/camera.h>
#include <memory>

namespace hotaru {

class Swapchain;

class ViewImpl : public View
{
public:
    explicit ViewImpl(std::unique_ptr<Swapchain> swapchain);
    ~ViewImpl() override;

    Camera* GetCamera() const override;
    Status Resize(const Extent& ext) override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}
