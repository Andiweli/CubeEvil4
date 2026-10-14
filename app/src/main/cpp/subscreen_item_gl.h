#pragma once
#include "scene.h"
#include "subscreen_data.h"
namespace re4 {
// Independent item-exam resources; opening the menu must not replace room GPU
// buffers, actors or simulation state.
void CreateSubscreenItemGl(const StaticScene &scene,
    const std::array<float,3> &rotationDegrees, float scale, unsigned id,
    std::shared_ptr<const RoomLighting> lighting = {}, unsigned spinAxis = 0,
    bool examine = false, const std::array<float,2> &frameY = {145,-35},
    const std::vector<SubscreenSpecular> &specular = {},
    const SubscreenArt &pickupFrame = {});
void ForgetSubscreenItemGl();
void DestroySubscreenItemGl();
// UI-safe normalized top-left rectangle; only Draw calls GLES.
void SetSubscreenItemViewport(const std::array<float,4> &normalizedBounds);
void DrawSubscreenItemGl(int width, int height, double seconds,
                         float lookX = 0, float lookY = 0);
} // namespace re4
