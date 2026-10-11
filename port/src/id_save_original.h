#pragma once
#include "re4/port/game_runtime.h"
#include "re4/port/id_system.h"
#include "re4/port/save_state.h"
#include <cstddef>
#include <cstdio>
#include <cstring>

namespace re4::port {
struct NativeInitLog { void err(int,int,const char *,...); };
struct IdSystemCpu {
    GameplayHeaps &heaps;
    explicit IdSystemCpu(GameplayHeaps &h):heaps(h) {}
    s32 m_maxId{}, m_nId{}, m_levelMax{};
    u32 m_set_flag[8]{}, m_disp_off[8]{};
    ID_UNIT *m_IdUnit{};
    GameplayHeaps::Allocation allocation;
    std::vector<std::string> trace;
    NativeInitLog log;
    NativeInitLog *pLog=&log;
    void *allocatePool(std::size_t,int,int);
    void Mem_free(void *);
    static void clearUnit(ID_UNIT *p) { *p=ID_UNIT{}; }
    static void memclr_asm(void *p,std::size_t n) { std::memset(p,0,n); }
    void gameInit(int);
    void roomInit();
    void free();
    void cockpitGameInit();
    int setCk(ID_CLASS);
    void dispSw(ID_CLASS,int);
    void unitPush(ID_UNIT *);
    ID_UNIT *unitPull();
    void unitLevel(ID_UNIT *,u8);
    void unitParent(ID_UNIT *,ID_UNIT *);
    ID_UNIT *unitPtr(u8,ID_CLASS);
    void kill(u8,ID_CLASS);
    void stop();
    void beMove(ID_UNIT *,int);
    void setTime(ID_UNIT *,s16);
};
struct RoomSaveCpu {
    GameplayHeaps &heaps;
    explicit RoomSaveCpu(GameplayHeaps &h):heaps(h) {}
    u16 m_RoomNum{},m_SaveNum{},m_RelNo{};
    void *m_pModule{},*m_pModule_bss{};
    RoomSaveHdr *m_pRoomSaveHead{};
    u8 *m_pRoomSaveData{};
    GameplayHeaps::Allocation allocation;
    std::vector<std::string> trace;
    void *mem_calloc(std::size_t,int,int);
    static void memclr_asm(void *p,std::size_t n) { std::memset(p,0,n); }
    void init();
    u32 getSaveDataSize();
    void save(void *);
    void load(void *);
    void clear(void *);
    u8 *getRoomSavePtr(u16);
    int checkRoomRange(u8,u8);
    int checkPassed(u16,int);
    void setPassed(u16,int);
};
struct SaveImageCpu {
    GameplayHeaps &heaps;
    RoomSaveCpu &RoomData;
    SaveImageCpu(GameplayHeaps &h,RoomSaveCpu &r):heaps(h),RoomData(r) {}
    struct ItemSize {
        int saveDataSize();
        std::function<void(void *)> writer;
        std::function<void(void *)> reader;
        void save(void *p) { writer(p); }
        void load(void *p) { reader(p); }
        [[noreturn]] int dumpType(int) { throw MissingGameConsumer("cGameSave::load/new round inventory"); }
    } ItemMgr;
    NativeGameState *pG{};
    struct UnboundActor {
        Vec pos{},ang{};
        [[noreturn]] UnboundActor *operator->() {
            throw MissingGameConsumer("cGameSave::save/live cPlayer position");
        }
    } pPL;
    std::function<void(SSCRN_SAVE_DATA *)> subscreenWriter;
    std::function<void(void *)> merchantWriter;
    std::function<void(SSCRN_SAVE_DATA *)> subscreenReader;
    std::function<void(void *)> merchantReader;
    std::function<int()> costume;
    void SscrnDataSave(SSCRN_SAVE_DATA *p) { subscreenWriter(p); }
    void MerchantDataSave(void *p) { merchantWriter(p); }
    void SscrnDataLoad(SSCRN_SAVE_DATA *p) { subscreenReader(p); }
    void MerchantDataLoad(void *p) { merchantReader(p); }
    int PlSetCostume() { return costume(); }
    [[noreturn]] void clearGlobalSaveData() { throw MissingGameConsumer("cGameSave::load/new round reset"); }
    [[noreturn]] void SndBgmTblInit() { throw MissingGameConsumer("SndBgmTblInit/original BGM table"); }
    [[noreturn]] void Merchant2ndRoundInit() { throw MissingGameConsumer("cGameSave::load/new round reset"); }
    bool save(SAVE_DATA_HEAD *,int);
    bool load(SAVE_DATA_HEAD *);
    std::vector<std::string> trace;
    std::vector<GameplayHeaps::Allocation> allocations;
    void *mem_calloc(std::size_t,int,int);
    int SscrnDataSize();
    int MerchantDataSize();
    SAVE_DATA_HEAD *alloc();
    void checkAddr(SAVE_DATA_HEAD *);
    void calcOffset(SAVE_DATA_HEAD *,std::uintptr_t);
    void calcAddr(SAVE_DATA_HEAD *);
};
} // namespace re4::port
