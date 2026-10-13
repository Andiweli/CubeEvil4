#include "runtime_original.h"
#include <algorithm>
#include <cmath>

namespace re4::port {
static_assert(sizeof(JOY) == 0x268 && sizeof(KEY) == 0xb8 && sizeof(PADStatus) == 12);
static_assert(std::is_trivially_copyable_v<JOY> && std::is_trivially_copyable_v<KEY>);
void ControllerInput::checkOwner() const {
    if (owner_ != std::this_thread::get_id())
        throw std::logic_error("controller owner thread required");
}
ControllerInput::Port *ControllerInput::find(int device) {
    checkOwner();
    if (device < 0)
        return nullptr;
    for (auto &port : ports_)
        if (port.device == device)
            return &port;
    return nullptr;
}
void ControllerInput::connect(int device, std::uint8_t port, bool analogLeft, bool analogRight) {
    if (device < 0 || port >= 4)
        throw std::invalid_argument("controller device/port");
    disconnect(device);
    ports_[port] = {};
    ports_[port].device = device;
    ports_[port].analogLeft = analogLeft;
    ports_[port].analogRight = analogRight;
}
void ControllerInput::disconnect(int device) {
    if (auto *port = find(device))
        *port = Port{};
}
bool ControllerInput::button(int device, int key, bool down) {
    auto *p = find(device);
    if (!p)
        return false;
    std::uint16_t mask = 0;
    // android.view.KeyEvent constants; physical Android buttons become GC PAD bits.
    // The original Key_type_tbl supplies all gameplay meanings afterwards.
    switch (key) {
    case 19:
        mask = JOY_UP;
        break;
    case 20:
        mask = JOY_DOWN;
        break;
    case 21:
        mask = JOY_LEFT;
        break;
    case 22:
        mask = JOY_RIGHT;
        break;
    case 96:
        mask = JOY_A;
        break;
    case 97:
        mask = JOY_B;
        break;
    case 99:
        mask = JOY_X;
        break;
    case 100:
        mask = JOY_Y;
        break;
    case 102:
        mask = JOY_L;
        break; // L1 aliases GC L; GC has only one left shoulder.
    case 103:
        mask = JOY_Z;
        break; // R1 -> GC Z.
    case 104:
        p->l2 = down;
        return true;
    case 105:
        p->r2 = down;
        return true;
    case 108:
        mask = JOY_START;
        break;
    // SELECT/BACK/stick clicks have no GC equivalent. Do not invent a gameplay action.
    default:
        return false;
    }
    if (down)
        p->buttons |= mask;
    else
        p->buttons &= static_cast<std::uint16_t>(~mask);
    return true;
}
bool ControllerInput::axes(int device, float x, float y, float z, float rz, float hx, float hy,
                           float lt, float rt) {
    auto *p = find(device);
    if (!p)
        return false;
    for (auto v : {x, y, z, rz, hx, hy, lt, rt})
        if (!std::isfinite(v))
            throw std::invalid_argument("nonfinite controller axis");
    p->x = std::clamp(x, -1.0f, 1.0f);
    p->y = std::clamp(y, -1.0f, 1.0f);
    p->z = std::clamp(z, -1.0f, 1.0f);
    p->rz = std::clamp(rz, -1.0f, 1.0f);
    p->lt = std::clamp(lt, 0.0f, 1.0f);
    p->rt = std::clamp(rt, 0.0f, 1.0f);
    p->hats = 0;
    if (hx < -0.5f)
        p->hats |= JOY_LEFT;
    if (hx > 0.5f)
        p->hats |= JOY_RIGHT;
    if (hy < -0.5f)
        p->hats |= JOY_UP;
    if (hy > 0.5f)
        p->hats |= JOY_DOWN;
    return true;
}
void ControllerInput::clearHeld() {
    checkOwner();
    for (auto &p : ports_) {
        auto device = p.device;
        auto left = p.analogLeft, right = p.analogRight;
        p = {};
        p.device = device;
        p.analogLeft = left;
        p.analogRight = right;
    }
}
OriginalPadSample ControllerInput::sample(std::uint8_t port) {
    checkOwner();
    if (port >= 4)
        throw std::out_of_range("controller port");
    auto &p = ports_[port];
    if (p.device < 0)
        return {};
    auto stick = [](float f) { return static_cast<s8>(std::lround(f * 100.0f)); };
    auto trigger = [](float f, bool digital, bool analog) {
        return static_cast<u8>(std::lround((analog ? f : (digital ? 1.0f : 0.0f)) * 255.0f));
    };
    return {static_cast<u16>(p.buttons | p.hats),
            stick(p.x),
            stick(-p.y),
            stick(p.z),
            stick(-p.rz),
            trigger(p.lt, p.l2, p.analogLeft),
            trigger(p.rt, p.r2, p.analogRight),
            0,
            0,
            0};
}
void ControllerInput::rumble(std::uint8_t port, bool enabled) {
    checkOwner();
    if (port >= 4)
        throw std::out_of_range("rumble port");
    ports_[port].motor = ports_[port].device >= 0 && enabled;
}
bool ControllerInput::requestedRumble(std::uint8_t port) const {
    checkOwner();
    if (port >= 4)
        throw std::out_of_range("rumble port");
    return ports_[port].motor;
}
void OriginalContext::PADRead(PADStatus *out) {
    for (u8 i = 0; i < 4; ++i) {
        auto p = pads.sample(i);
        out[i] = {p.button,      p.stickX,       p.stickY,  p.substickX, p.substickY,
                  p.triggerLeft, p.triggerRight, p.analogA, p.analogB,   p.error};
    }
}
PadRuntime::PadRuntime(PlatformGamepads &pads)
    : original_(std::make_unique<OriginalContext>(pads)) {
    original_->PadInit();
}
PadRuntime::~PadRuntime() = default;
void PadRuntime::tick(int vcnt) {
    original_->SetSystemVcnt(vcnt);
    original_->PadRead();
}
void PadRuntime::clear() {
    for (auto &joy : original_->Joy)
        joy = JOY{};
    original_->Key = KEY{};
    original_->un_stop_mask = 0;
    original_->Vib_level = 0;
    for (u8 i = 0; i < 4; ++i)
        original_->pads.rumble(i, false);
}
const JOY &PadRuntime::joy(std::uint8_t port) const {
    if (port >= 4)
        throw std::out_of_range("Joy port");
    return original_->Joy[port];
}
const KEY &PadRuntime::key() const { return original_->Key; }
NativeGameState &PadRuntime::state() { return original_->global; }
void PadRuntime::stopKeys(u64 permitted) { original_->KeyStop(permitted); }
} // namespace re4::port
