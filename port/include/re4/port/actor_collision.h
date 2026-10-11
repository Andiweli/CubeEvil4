#pragma once
#include "re4_runtime_types.h"
namespace re4::port {
struct ModelCpu;
// Named fields of include/atariInfo.h. The GC x48 union is a native pointer,
// not a four-byte overlay. This object is never read directly from disc bytes.
struct ActorCollisionInfo {
    Vec m_offset{};
    f32 m_radius{},m_radius2{},m_height{};
    s16 m_parts_no{};
    u16 m_flag{};
    f32 m_radius_n{},m_radius2_n{};
    u16 m_hokan{},m_stat{};
    ModelCpu *m_pMod{};
    f32 m_radius3{};
    Vec m_Pos{},m_oldPos{};
    ActorCollisionInfo *m_pList{};
    ActorCollisionInfo();
    void init0(f32,f32,f32,f32,f32,f32,f32,int,int,int);
    void init(f32,f32,f32,f32,f32,f32,f32,int,int,int);
    void set(f32,f32,int);
    void move();
    void getSpeedVector(ModelCpu*,Vec*,Vec*);
    void getPos(ModelCpu*,Vec*);
    void setPriority(int);
    void on() { m_flag|=0x300; }
    void off() { m_flag&=static_cast<u16>(~0x300); }
    void onSca() { m_flag|=0x100; }
    void offSca() { m_flag&=static_cast<u16>(~0x100); }
    void onOba() { m_flag|=0x200; }
    void offOba() { m_flag&=static_cast<u16>(~0x200); }
};
}
