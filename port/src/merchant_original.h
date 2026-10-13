#pragma once
#include "re4/port/merchant.h"
#include "re4/port/subscreen.h"
#include <cstring>
#include <stdexcept>

namespace re4::port {
u8 itemType(u16);
struct MerchantDomain {
    NativeGameState *pG;
    struct System { u8 &language; } system;
    System *pSys=&system;
    MERCHANT_DATA merchantData[1]{};
    MerchantCharacter merchantChar{};
    struct Log {
        [[noreturn]] void err(int,int,const char *message) { throw std::runtime_error(message); }
    } log;
    Log *pLog=&log;
    MerchantDomain(NativeGameState &g,u8 &language):pG(&g),system{language} {}
    u32 chkFlag6C(u32);
    void merchant_stage1_full();
    void merchant_stage2_full();
    void merchant_stage3_full();
    void MerchantGameInit();
    void Merchant2ndRoundInit();
    void stockDataInit(MERCHANT_DATA *);
    void add_stock(StockEntry *,StockEntry *);
    void stockDataAdd(MERCHANT_DATA *,StockEntry *);
    void levelDataInit(MERCHANT_DATA *);
    void levelDataAdd(MERCHANT_DATA *,LevelEntry *);
    void MerchantDataSave(void *);
    void MerchantDataLoad(void *);
    template<class T> void memclr_asm(T *p,std::size_t bytes) {
        if(bytes%sizeof(T))throw std::logic_error("merchant typed clear extent");
        for(std::size_t n=0;n<bytes/sizeof(T);++n)p[n]=T{};
    }
};
} // namespace re4::port
#define MERCHANT_NUM 1
#define STOCK_MAX 64
#define LEVEL_MAX 32
