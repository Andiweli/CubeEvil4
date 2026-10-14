#pragma once
#include <array>
#include <cstdint>
namespace re4 {
constexpr int DefaultDisplayBrightness=64;
constexpr int MinimumDisplayBrightness=34;
constexpr int MaximumDisplayBrightness=114;
// main_sub.cpp::Bg_brightness_set, including its inverse order[] priority.
std::array<std::uint8_t,7> DisplayBrightnessFilter(int brightness);
} // namespace re4
