#pragma once
#include "object_original.h"
#include "re4/port/weapons.h"
#include "re4/port/byte_order.h"
#include <array>

namespace re4::port {
struct WeaponFileEntry {char name[64]{};};
struct WeaponReadFile {u16 file{},dll{};u32 flag{};};
struct WeaponReadInfo {void *start_addr[2][32]{};u32 read_size[2][32]{};};
struct WeaponRelView {u32 bssSize{},offset{};ModuleMetadata metadata;RelModuleIndex index;};
extern const char *OriginalWeaponFileNames[];
extern const std::size_t OriginalWeaponFileCount;
struct WeaponDomain:std::enable_shared_from_this<WeaponDomain> {
    GameRuntime &runtime;
    ModuleRuntime &modules;
    ObjectRuntime &objects;
    NativeGameState *pG;
    std::vector<WeaponFileEntry> files;
    struct Request {LoadRequest read;GameplayHeaps::Allocation target;};
    std::map<int,Request> requests;
    int nextRequest=1;
    GameplayHeaps::Allocation regularBuffer,alternateBuffer;
    std::shared_ptr<const LoadedFile> loaded;
    ResourceLease body,sound;
    AssetIndex index;
    WeaponRelView rel;
    u32 currentModule{},mappedWeapon{};
    std::function<void(PlayerDomain*)> WeaponInitFunc,WeaponMoveFunc;
    std::array<std::function<NativeObject*(NativeObject*)>,0x40> ObjInitFunc;
    std::array<void*,3> PlWepMot{};
    bool initialized{};
    PlayerDomain *activePlayer{};
    std::vector<std::string> trace;
    std::vector<u32> effectLoads;
    WeaponDomain(GameRuntime &,PlayerDomain &);
    void installModules();
    void clearBindings(u32);
    void registerProlog(ModuleContext &,u32,const TaskEntry &);
    NativeObject *construct(NativeObject*,u32);
    NativeWeapon *createBack(u32);
    struct ObjectFacade {
        WeaponDomain *d;
        NativeWeapon *createBack(u32 id){return d->createBack(id);}
    } ObjMgr{this};
    struct Log {[[noreturn]] void err(int,int,const char *m){throw std::runtime_error(m);}} log;
    Log *pLog=&log;
    void OSReport(const char* m){ModuleContext::active().observe(m);}
    void PlHandgunMove(PlayerDomain*);
    void EspDataLoad(void*,u32,int);
    void Wep01_init(PlayerDomain*);void Wep02_init(PlayerDomain*);
    void prolog01();void epilog01();void prolog02();void epilog02();
    NativeObject *ObjFn57_init(NativeObject*);
    NativeObject *ObjRuger_init(NativeObject*);
    NativeObject *ObjMauser_init(NativeObject*);
    void *arc(u32) const;
    ResourceLease lease(void*) const;
    u8 *dataAddress(bool);
    int read(const char*,void*,u32,u32,u32,u32);
    int readCheck(int,WeaponReadInfo*);
    WeaponRelView *embedded(void*);
    void link(WeaponRelView*,void*);
    void unlink(void*);
    void bindInitCallback(PlayerDomain &);
    void clearRead();
    void clear();
    WeaponReadSnapshot snapshot() const;
    void invokeInit(PlayerDomain &);
    static void setDrsName(char*);
    ModelInfoView *initializeObjectModel(NativeWeapon &,void*,void*);
    void bindParent(NativeWeapon &,ModelCpu*);
    void setMotion(NativeWeapon &,void*,u8,u16,u16,void*);
    int moveMotion(NativeWeapon &);
};
} // namespace re4::port
