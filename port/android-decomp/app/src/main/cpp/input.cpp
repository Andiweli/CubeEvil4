#include "input.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
namespace re4 {
static float finite(float v) {
  return std::isfinite(v) ? std::clamp(v, -1.0f, 1.0f) : 0.0f;
}
static void stick(float x, float y, float &outX, float &outY) {
  x = finite(x);
  y = finite(y);
  const float n = std::sqrt(x * x + y * y);
  constexpr float dead = 0.15f;
  if (n <= dead) {
    outX = outY = 0;
    return;
  }
  const float scaled = (std::min(n, 1.0f) - dead) / (1.0f - dead);
  outX = x / n * scaled;
  outY = y / n * scaled;
}
InputFrame InputMapper::poll(const RawInput &r) {
  InputFrame f;
  stick(r.lx, r.ly, f.lx, f.ly);
  stick(r.rx, r.ry, f.rx, f.ry);
  // Java canonicalizes Y: physical up is positive. Reverse only horizontal
  // RS once; camera, gun and knife all receive the same natural vertical axis.
  // Keep neutral positive zero so controller diagnostics do not show -0.00.
  f.rx = f.rx == 0 ? 0 : -f.rx;
  f.ry = f.ry == 0 ? 0 : f.ry;
  f.lt = std::max(0.0f, finite(r.lt));
  f.rt = std::max(0.0f, finite(r.rt));
  ltHeld_ = (r.buttons & Lt) || f.lt > (ltHeld_ ? 0.35f : 0.55f);
  rtHeld_ = (r.buttons & Rt) || f.rt > (rtHeld_ ? 0.35f : 0.55f);
  auto add = [&](bool on, std::uint64_t a) {
    if (on)
      f.held |= a;
  };
  add(r.buttons & Bottom, Confirm | Interact);
  add(r.buttons & Right, Cancel | Run);
  add(r.buttons & Left, Reload);
  add(r.buttons & Top, Inventory);
  add(r.buttons & Lb, Knife);
  add(r.buttons & Rb, Ashley);
  add(r.buttons & Start, Pause);
  add(r.buttons & Select, Map);
  add(ltHeld_, Aim);
  add(rtHeld_, Fire);
  bool u = (r.buttons & Up) || f.ly > 0.5f,
       d = (r.buttons & Down) || f.ly < -0.5f;
  bool l = (r.buttons & DLeft) || f.lx < -0.5f,
       rr = (r.buttons & DRight) || f.lx > 0.5f;
  add(u && !d, MenuUp);
  add(d && !u, MenuDown);
  add(l && !rr, MenuLeft);
  add(rr && !l, MenuRight);
  // Resistance has its own hysteresis: smaller deliberate stick flicks count,
  // while held directions and noise around neutral do not repeat.
  auto direction = [](float value, int old) {
    if (value >= .25f)
      return 1;
    if (value <= -.25f)
      return -1;
    if (old > 0 && value > .10f)
      return 1;
    if (old < 0 && value < -.10f)
      return -1;
    return 0;
  };
  resistanceX_ = direction(f.lx, resistanceX_);
  resistanceY_ = direction(f.ly, resistanceY_);
  std::uint64_t resistance = f.held & (Confirm | Interact | Cancel | Run);
  auto resistDirection = [&](bool positive, bool negative, std::uint64_t pos,
                             std::uint64_t neg) {
    if (positive != negative)
      resistance |= positive ? pos : neg;
  };
  resistDirection((r.buttons & DRight) || resistanceX_ > 0,
                  (r.buttons & DLeft) || resistanceX_ < 0, MenuRight, MenuLeft);
  resistDirection((r.buttons & Up) || resistanceY_ > 0,
                  (r.buttons & Down) || resistanceY_ < 0, MenuUp, MenuDown);
  const auto edges = resistance & ~resistancePrevious_;
  resistancePrevious_ = resistance;
  // Original PlGachaMove groups: horizontal direction contributes twice.
  f.resistancePresses =
      unsigned(bool(edges & (MenuUp | MenuDown | MenuLeft | MenuRight))) +
      unsigned(bool(edges & (Confirm | Interact | Cancel | Run))) +
      unsigned(bool(edges & (MenuLeft | MenuRight)));
  f.pressed = f.held & ~previous_;
  f.released = previous_ & ~f.held;
  previous_ = f.held;
  return f;
}
std::string Describe(const InputFrame &f) {
  const std::pair<std::uint64_t, const char *> names[] = {
      {Confirm, "Confirm"}, {Cancel, "Back / Run"},
      {Interact, "Action"},    {Aim, "Aim"},
      {Fire, "Fire"},        {Knife, "Knife"},
      {Reload, "Reload"},   {Inventory, "Inventory"},
      {Ashley, "Ashley"},      {Pause, "Pause"},
      {Map, "Map"},          {MenuUp, "Up"},
      {MenuDown, "Down"},    {MenuLeft, "Left"},
      {MenuRight, "Right"}};
  std::ostringstream s;
  s << std::fixed << std::setprecision(2) << "Left " << f.lx << ", " << f.ly
    << "  Right " << f.rx << ", " << f.ry << "\nLT " << f.lt << "  RT " << f.rt
    << "\n";
  for (auto n : names)
    if (f.held & n.first)
      s << n.second << " · ";
  return s.str();
}
void InputBuffer::submit(const RawInput &raw) {
  latest_ = mapper_.poll(raw);
  resistancePresses_ =
      std::min(64u, resistancePresses_ + latest_.resistancePresses);
  pressed_ |= latest_.pressed;
  released_ |= latest_.released;
}
InputFrame InputBuffer::consume() {
  InputFrame f = latest_;
  f.resistancePresses = resistancePresses_;
  resistancePresses_ = 0;
  f.pressed = pressed_;
  f.released = released_;
  pressed_ = released_ = 0;
  return f;
}
void InputBuffer::reset() {
  resistancePresses_ = 0;
  mapper_.reset();
  latest_ = {};
  pressed_ = released_ = 0;
}
} // namespace re4
