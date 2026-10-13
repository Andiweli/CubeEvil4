#ifndef TYPES_H
#define TYPES_H

// Native replacement for include/types.h. Only primitive widths are adapted;
// this header does not assert compatibility of native classes or file layouts.
#include <cstdint>
#include <limits>

using s8 = std::int8_t;
using s16 = std::int16_t;
using s32 = std::int32_t;
using s64 = std::int64_t;
using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using f32 = float;
using f64 = double;
using BOOL = int;

static_assert(sizeof(s8) == 1 && sizeof(u8) == 1);
static_assert(sizeof(s16) == 2 && sizeof(u16) == 2);
static_assert(sizeof(s32) == 4 && sizeof(u32) == 4);
static_assert(sizeof(s64) == 8 && sizeof(u64) == 8);
static_assert(sizeof(BOOL) == 4);
static_assert(sizeof(f32) == 4 && std::numeric_limits<f32>::is_iec559);
static_assert(sizeof(f64) == 8 && std::numeric_limits<f64>::is_iec559);
static_assert(sizeof(void*) == 4 || sizeof(void*) == 8,
    "Native addresses must be represented by uintptr_t, never u32");

#endif
