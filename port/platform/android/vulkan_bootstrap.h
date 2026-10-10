#pragma once
#include "re4/port/graphics.h"
#include <android/native_window.h>
#include <string>
namespace re4::port {
// API 1.0 clear/present subset, loaded only when an Android surface is attached.
class VulkanBootstrap final : public GraphicsPresenter {
public:
    explicit VulkanBootstrap(ANativeWindow *);
    ~VulkanBootstrap() override;
    void present(const GraphicsClear &,bool) override;
    std::string report() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace re4::port
