// Literal upstream moveEyeNormal and cDelay; only the three static storage
// declarations are moved to owner fields to make independent source fixtures.
#pragma once
#include "player_eyes.h"
#include <array>
#include <cstdint>
namespace player_eye_oracle {
using f32=float;
template <class T> class cDelay {
    T m_Val0;
    T m_Val1;
    T m_Delay;

public:
    T current() const {return m_Val0;}
    T target() const {return m_Val1;}
    T delay() const {return m_Delay;}
    void restore(T a,T b,T d) {m_Val0=a;m_Val1=b;m_Delay=d;}
    cDelay() { m_Val0 = m_Val1 = m_Delay = 0.0f; }
    void setDelay(T delay) { m_Delay = delay; }
    void reset(T v) { m_Val1 = v; m_Val0 = v; }
    void limit(T lo, T hi)
    {
        if (m_Val1 < lo) m_Val1 = lo;
        else if (m_Val1 > hi) m_Val1 = hi;
        if (m_Delay == 0.0f) m_Val0 = m_Val1;
    }
    cDelay& operator=(T v)
    {
        m_Val1 = v;
        if (m_Delay == 0.0f) m_Val0 = m_Val1;
        return *this;
    }
    cDelay& operator+=(T v)
    {
        m_Val1 += v;
        if (m_Delay == 0.0f) m_Val0 = m_Val1;
        return *this;
    }
    cDelay& operator-=(T v)
    {
        m_Val1 -= v;
        if (m_Delay == 0.0f) m_Val0 = m_Val1;
        return *this;
    }
    void move() { m_Val0 = m_Val0 * m_Delay + m_Val1 * (1.0f - m_Delay); }
    operator T() { return m_Val0; }
};
typedef cDelay<f32> cDelayF;
struct Ang {float x{},y{},z{};};
struct cParts {Ang ang;void matUpdate() {}};
class OriginalEyeOracle {
public:
 cDelayF eyeDir; int timer{},eyetime{}; std::uint16_t seed{}; unsigned draws{};
 std::array<cParts,34> parts;
 unsigned Rnd() {
  ++draws;const unsigned n=(unsigned(std::uint8_t((seed>>1)+(seed>>8)))<<8)|std::uint8_t(seed>>1);
  seed=std::uint16_t(n==seed?n+0x101:n);return std::uint8_t(seed>>8);
 }
 cParts *getPartsPtr(unsigned n) {return &parts[n];}
 void moveEyeNormal();
 void restore(const re4::PlayerEyeState &s) {eyeDir.restore(s.direction,s.target,s.delay);timer=s.blinkTimer;eyetime=s.wanderTimer;}
 re4::PlayerEyeState state() {return {eyeDir.current(),eyeDir.target(),eyeDir.delay(),timer,eyetime};}
};
inline void OriginalEyeOracle::moveEyeNormal()
{
    cParts* p;

    p = getPartsPtr(0x1C);
    // Every case written out separately in ascending order: jump2 cross-jumps the identical
    // bodies into the last copy, which gives the target's body layout (3, 4, 0x1E, 0x58, 0x5A,
    // 0x5D, 0x5E, 0x5F, 0x60, 0x61, 0x62) and constant-pool order.
    switch (timer++) {
    default:
        p->ang.x = 0.0f;
        break;
    case 0: {
        eyeDir = ((f32) (Rnd() % 200) * 0.01f - 1.0f) * 3.1415927f * 0.1f;
        p->ang.x = 0.0872664600610733f;
        break;
    }
    case 1:
        p->ang.x = 0.1745329201221466f;
        break;
    case 2:
        p->ang.x = 0.3490658402442932f;
        break;
    case 3:
        p->ang.x = 0.3141592741012573f;
        break;
    case 4:
        p->ang.x = 0.24434609711170197f;
        break;
    case 5:
        p->ang.x = 0.1745329201221466f;
        break;
    case 6:
        p->ang.x = 0.0872664600610733f;
        break;
    case 0x1E:
        eyeDir = 0.0f;
        break;
    case 0x58:
        if (Rnd() & 3) {
            timer = 0;
        } else {
            timer = 0x5A;
        }
        break;
    case 0x5A:
        p->ang.x = 0.0872664600610733f;
        break;
    case 0x5B:
        p->ang.x = 0.1745329201221466f;
        break;
    case 0x5C:
        p->ang.x = 0.3490658402442932f;
        break;
    case 0x5D:
        p->ang.x = 0.296705961227417f;
        break;
    case 0x5E:
        p->ang.x = 0.33161255717277527f;
        break;
    case 0x5F:
        p->ang.x = 0.3490658402442932f;
        break;
    case 0x60:
        p->ang.x = 0.2617993950843811f;
        break;
    case 0x61:
        p->ang.x = 0.1745329201221466f;
        break;
    case 0x62:
        p->ang.x = 0.0872664600610733f;
        timer = 10;
        break;
    }
    p->matUpdate();
    {

        if (--eyetime < 0) {
            eyeDir += ((f32) (Rnd() % 200) * 0.01f - 1.0f) * 0.03141592815518379f;
            eyetime = Rnd() % 3 + 2;
        }
    }
    eyeDir.limit(-0.3141592741012573f, 0.3141592741012573f);
    p = getPartsPtr(0x20);
    p->ang.y = eyeDir;
    p->matUpdate();
    p = getPartsPtr(0x21);
    p->ang.y = eyeDir;
    p->matUpdate();
    eyeDir.move();
}

} // namespace player_eye_oracle
