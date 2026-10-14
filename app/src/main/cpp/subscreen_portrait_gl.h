#pragma once
#include "subscreen_data.h"
namespace re4 {
// GL-owner only; source SS model/materials at the requested offscreen size.
// The Android bridge uses authored 640x480 and interpolates solved poses,
// preserving the paused room's buffers, simulation and light state.
Texture RenderSubscreenPortraitGl(const OriginalMenuModel &model,int width,int height);
void DestroySubscreenPortraitGl(const OriginalMenuModel *model=nullptr);
void ForgetSubscreenPortraitGl();
}
