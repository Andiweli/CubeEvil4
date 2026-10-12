#pragma once
#include "re4/port/game_runtime.h"
#include "re4/port/subscreen.h"
#include <cstring>
#include <cstdio>
#include <map>
#include <cstddef>

namespace re4::port {
static_assert(offsetof(GameSaveBlock,pad_8304)+sizeof(GameSaveBlock::pad_8304)-offsetof(GameSaveBlock,ope_x82E8)==0x44,
              "original Ope memset span must remain pointer-free and exactly 68 bytes");
// Original named native fields, not an overlay of the GC SUB_SCREEN.
struct SubScreenWorkCpu {
    char filename[0x28]{};
    SS_OPEN_FLAG open_flag=SS_OPEN_NULL;
    SS_ATTR_FLAG attr_flag=SS_ATTR_NULL;
    s32 close_flag{},wait_cnt{};
    u32 pFreeOffs{},pPreplfOffs{},pCommonOffs{},pSwitchOffs{};
    std::uintptr_t relAddr{};
    u32 p_module{}; // Logical ARAM address, never a dereferenceable native pointer.
    u8 board_next{},board_size{},map_mode{};
    u32 save{};
};
using SubScreenWork=SubScreenWorkCpu;
struct MapManagerCpu {
    void *pArray{}, *pAlive{}, *pAlivePush{}, *pArrayPush{};
    u32 nArray{},nArrayPush{},maxAlive{};
    int roomInit(); // Entire original cManager<T>::roomInit body.
};
void itemInfo(ITEM_ID,ITEM_INFO *);
u8 itemType(ITEM_ID);
u8 itemDefNum(ITEM_ID);
u16 itemMaxNum(ITEM_ID);
void setBullet(cItem *,u16);
struct InventoryCpu {
    GameplayHeaps &heaps;
    NativeGameState *pG;
    InventoryCpu(GameplayHeaps &h,NativeGameState &g):heaps(h),pG(&g) {}
    u32 *m_pAvailable{};
    s32 m_flag_num{};
    u16 used_id{};
    cItem *m_pWep{};
    u16 m_wep_id{};
    s8 m_to_whom{};
    u8 m_char{};
    cItem *m_pItem{}, *m_pNew{};
    s32 m_array_num{};
    ITEM_ORDER *m_p_order_tbl{};
    s32 m_order_tbl_num{};
    u32 m_bonus_time{},m_bonus_point{};
    std::vector<GameplayHeaps::Allocation> allocations;
    struct Log { void err(int,int,const char *format,...); } log;
    Log *pLog=&log;
    int init();
    void clear();
    void flagclear();
    void roomInit();
    cItem *search(u16);
    cItem *at(int);
    int searchAt(cItem *);
    void construct(cItem *,ITEM_ID);
    int get(ITEM_ID,int);
    int countFiles();
    void gameInit();
    int set_game(int);
    [[noreturn]] int set_ada(int) { throw MissingGameConsumer("cItemMgr::set_ada/PutInCase"); }
    [[noreturn]] int set_char(int) { throw MissingGameConsumer("cItemMgr::set_char/PutInCase"); }
    cItem *newbie() { return m_pNew; } // Original inline include/item.h.
    cItem *weapon() { return m_pWep; }
    u16 weaponId() { return m_wep_id; }
    u16 weaponId(cItem *);
    u16 num(int,u8);
    u16 num(int);
    int arm(cItem *);
    u16 bulletNum();u16 bulletNumCurrent();u16 bulletNum(cItem*);u16 bulletNum(ITEM_ID);
    int dump(ITEM_ID);
    int dump(cItem *);
    void erase(cItem *);
    InventoryCpu &ItemMgr=*this; // Original manager global is this owned instance.
    void save(void *);
    void load(void *);
    static void memclr_asm(ItemSaveWork *p,std::size_t n) {
        if(n!=sizeof(*p))throw std::invalid_argument("original item save clear extent");
        *p=ItemSaveWork{};
    }
    u16 WeaponId2ChargeNum(ITEM_ID,int);
    int addMoney(int);
    [[noreturn]] void MercSysSetAddTime(int) { throw MissingGameConsumer("MercSysSetAddTime"); }
    [[noreturn]] void MercSysSetBonusTime(int) { throw MissingGameConsumer("MercSysSetBonusTime"); }
    void *mem_alloc(std::size_t,int,int);
    cItem *allocateItems(int);
    ITEM_ORDER *allocateOrders(int);
    void Mem_free(void *);
    void reset();
};
struct SubScreenContext {
    EngineLoader &loader;
    GameplayHeaps &heaps;
    NativeGameState *pG;
    struct System { u8 &language; } system;
    System *pSys=&system;
    std::shared_ptr<const ImportedFiles> files;
    u8 disc;
    InventoryCpu ItemMgr;
    MapManagerCpu MapMgr;
    SubScreenWork SubScreenWk;
    std::vector<SubScreenResource> loaded;
    bool initialized=false,inventoryReady=false;
    const std::thread::id owner=std::this_thread::get_id();
    std::vector<std::string> trace;
    // Sync mode 9 cannot yield an abandoned stack: ledger is still owner-held
    // until ReadCheck consumes the finished request and DMA readback.
    struct Read { LoadRequest request; u32 address; };
    std::map<int,Read> reads;
    int nextRead=1;
    struct DvdFacade {
        SubScreenContext *context;
        int ReadCheck(int,int *,int *,u32 *);
    } Dvd{this};
    SubScreenContext(EngineLoader &l,GameplayHeaps &h,NativeGameState &g,u8 &language,
                     std::shared_ptr<const ImportedFiles> f,u8 d)
        :loader(l),heaps(h),pG(&g),system{language},files(std::move(f)),disc(d),ItemMgr(h,g) {}
    void check(bool task=false) const;
    int DvdReadN(const char *,void *,u32,int,int,int);
    void SubScreenAramRead();
    void sscrnSetLanguage(SubScreenWork *,int);
    void sscrnDataFilename(SubScreenWork *,const char *);
    void SubScreenGameInit();
    void SubScreenRoomInit();
    void SubScreenWait(int);
    void SscrnDataSave(SSCRN_SAVE_DATA *);
    void SscrnDataLoad(SSCRN_SAVE_DATA *);
    int PlSetCostume();
    void OSReport(const char *,...) const;
};
#define StaFlagOn(g,n) ((g)->Status_flg[(n)/32] |= (0x80000000u >> ((n)%32)))
#define StaFlagOff(g,n) ((g)->Status_flg[(n)/32] &= ~(0x80000000u >> ((n)%32)))
#define ScfFlagChk(g,n) ((g)->Scenario_flg[(n)/32] & (0x80000000u >> ((n)%32)))
} // namespace re4::port
#define SysFlagChk(g,n) ((g)->System_flg & (0x80000000u >> ((n)%32)))
#define DbgFlagChk(g,n) ((g)->Debug_flg[(n)/32] & (0x80000000u >> ((n)%32)))
#define FlagChkSign(flg,n) ((s32)((flg)[(n)/32] << ((n)%32)) < 0)
#define FlagChkSignW(flg,n) ((s32)((flg) << ((n)%32)) < 0)
