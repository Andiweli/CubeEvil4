#pragma once
#include <cstdint>
#include <string>
namespace re4 {
enum Button : std::uint32_t {
  Bottom = 1u << 0,
  Right = 1u << 1,
  Left = 1u << 2,
  Top = 1u << 3,
  Lb = 1u << 4,
  Rb = 1u << 5,
  Start = 1u << 6,
  Select = 1u << 7,
  Up = 1u << 8,
  Down = 1u << 9,
  DLeft = 1u << 10,
  DRight = 1u << 11,
  Lt = 1u << 12,
  Rt = 1u << 13
};
enum Action : std::uint64_t {
  Confirm = 1ull << 0,
  Cancel = 1ull << 1,
  Interact = 1ull << 2,
  Run = 1ull << 3,
  Aim = 1ull << 4,
  Fire = 1ull << 5,
  Knife = 1ull << 6,
  Reload = 1ull << 7,
  Inventory = 1ull << 8,
  Ashley = 1ull << 9,
  Pause = 1ull << 10,
  Map = 1ull << 11,
  MenuUp = 1ull << 12,
  MenuDown = 1ull << 13,
  MenuLeft = 1ull << 14,
  MenuRight = 1ull << 15
};
struct RawInput {
  std::uint32_t buttons{};
  // Controller axes: horizontal positive is physical right, vertical is up.
  float lx{}, ly{}, rx{}, ry{}, lt{}, rt{};
};
struct InputFrame {
  unsigned resistancePresses{};
  std::uint64_t held{}, pressed{}, released{};
  // Right X is the shared camera/aim yaw axis: physical right maps negative.
  // Left axes retain physical directions; right vertical is positive for up.
  float lx{}, ly{}, rx{}, ry{}, lt{}, rt{};
};
class InputMapper {
  std::uint64_t previous_{}, resistancePrevious_{};
  int resistanceX_{}, resistanceY_{};
  bool ltHeld_{}, rtHeld_{};

public:
  InputFrame poll(const RawInput &raw);
  void reset() {
    previous_ = resistancePrevious_ = 0;
    resistanceX_ = resistanceY_ = 0;
    ltHeld_ = rtHeld_ = false;
  }
};
// Latch edges between render frames so short presses cannot disappear.
class InputBuffer {
  InputMapper mapper_;
  InputFrame latest_;
  std::uint64_t pressed_{}, released_{};
  unsigned resistancePresses_{};

public:
  void submit(const RawInput &raw);
  InputFrame consume();
  void reset();
};
std::string Describe(const InputFrame &frame);
} // namespace re4
