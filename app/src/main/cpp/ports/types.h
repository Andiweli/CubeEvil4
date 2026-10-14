#pragma once
#include <cstdint>
#include <cstring>
#include <limits>
using s8 = std::int8_t;
using u8 = std::uint8_t;
using s16 = std::int16_t;
using u16 = std::uint16_t;
using s32 = std::int32_t;
using u32 = std::uint32_t;
using s64 = std::int64_t;
using u64 = std::uint64_t;
using f32 = float;
using f64 = double;
using BOOL = int;
static_assert(sizeof(u32) == 4 && sizeof(u64) == 8 && sizeof(f32) == 4);
static_assert(std::numeric_limits<f32>::is_iec559, "IEEE 754 float required");
inline f32 FloatFromBits(u32 value) {
  f32 f;
  std::memcpy(&f, &value, sizeof f);
  return f;
}
