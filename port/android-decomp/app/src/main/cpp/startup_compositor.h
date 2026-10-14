#pragma once
#include <cstddef>
#include <cstdint>
namespace re4 {
// Pure bounded title EFB operation. No renderer, sound, asset mutex or global scene access.
// parameters[25]: texture w/h, mask w/h, inverse affine2x3,
// local l/t/r/b, tintRGB, alpha/type/sub/blend, clipped x0/y0/x1/y1.
// An optional final l/t/r/b restricts the copied EFB to stretched content.
void ComposeStartupFrame(std::uint32_t *framebuffer,std::size_t frameCount,
    const std::uint32_t *texture,std::size_t textureCount,
    const std::uint32_t *mask,std::size_t maskCount,
    const float *parameters,std::size_t parameterCount);
}
