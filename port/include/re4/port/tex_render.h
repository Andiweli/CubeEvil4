#pragma once
#include "re4_runtime_types.h"
#include "re4/port/engine_loader.h"
#include <array>
#include <functional>
#include <map>
#include <thread>
namespace re4::port {
class TexRenderRuntime;
class TexRenderTarget {
public:
    TexRenderRuntime *owner{};
    void Init(); int AllocBuf(); void ReAllocBuf();
    int IsAlive() const { return m_Be_flg; }
    void SetAlive(int a) { m_Be_flg=a; }
    void *GetBufAddr() const { return m_Texture_buffer; }
    void SetTexNo(u8 n) { m_Tex_no=n; } u8 GetTexNo() const { return m_Tex_no; }
    void SetCoreFlg(u16 n) { m_Core_flg=n; } u16 GetCoreFlg() const { return m_Core_flg; }
    void SetWHSize(u32 w,u32 h) { m_W_size=w; m_H_size=h; }
    u32 GetWSize() const { return m_W_size; } u32 GetHSize() const { return m_H_size; }
    u32 GetTexBufSize() const;
    void SetRepeatType(int t) { m_Rep_type=t; } int GetRepeatType() const { return m_Rep_type; }
private:
    int m_Be_flg{};
    // GXTexObj is created by CopyTexRenderMgr, not by TexRenderInit. No fake GPU object.
    void *m_Texture_buffer{};
    u8 m_Tex_no{},pad{}; u16 m_Core_flg{};
    u32 m_W_size{},m_H_size{}; int m_Rep_type{};
};
class TexRenderRuntime final {
public:
    explicit TexRenderRuntime(std::function<MemoryReservation(u32)>);
    ~TexRenderRuntime();
    void clear();
    TexRenderTarget *initialize(int size,int repeat);
    const TexRenderTarget &target(u32) const;
    u32 count() const { check(); return g_RndMgrNum; }
    std::size_t bufferBytes() const;
    [[noreturn]] void copyToTexture(); // Phase-5 backend required; never reports rendered success.
    void TexRenderMgrInit(); void TexRenderMgrRoomInit();
    int GetTexRenderMgr(TexRenderTarget **);
    void TexRenderInit(TexRenderTarget **,int,int);
    void *allocate(TexRenderTarget *,u32);
    void release(void *);
private:
    void check() const;
    std::thread::id owner_=std::this_thread::get_id();
    std::function<MemoryReservation(u32)> allocate_;
    std::map<void *,MemoryReservation> buffers_;
    std::array<TexRenderTarget,8> g_RndMgr{};
    u32 g_RndMgrNum{}; int g_draw{};
};
} // namespace re4::port
