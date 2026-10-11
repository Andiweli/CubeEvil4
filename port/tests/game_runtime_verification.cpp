#include "consumer_verification.h"
#include "model_verification.h"
#include "object_verification.h"
#include "light_parent_verification.h"
#include "re4/port/game_runtime.h"
#include "re4/port/sha256.h"
#include "re4_scheduler_original.h"
#include <algorithm>
#include <filesystem>
#include <cstring>
#include <fstream>
#include <limits>
#include <sstream>
#include <cmath>

namespace re4::port {
// Signatures from src/game/ik.cpp; numeric fixtures are test inputs, never game data.
void IKInit(ModelCpu*,ModelMotion*);
void ikCalc(ModelPart*,ModelPart*,ModelPart*);
namespace {
struct Checks {
    int count = 0;
    void test(bool condition, const char *why) {
        ++count;
        if (!condition)
            throw std::runtime_error(why);
    }
    template <class F> void rejects(F callback, const char *why) {
        bool rejected = false;
        try {
            callback();
        } catch (const std::exception &) {
            rejected = true;
        }
        test(rejected, why);
    }
};
// Numeric adapter fixture: original triangle/block kernels run on checked BE
// storage. This geometry is solely a collision test, never a room resource.
void sphereTests(Checks &c,GameRuntime &game) {
    using namespace collision;
    auto fixture=[](u32 attr) {
        std::vector<u8> b(138);b[0]=0xff;b[3]=3;b[5]=1;b[7]=1;b[11]=1;b[19]=1;
        auto scalar=[&](u32 at,f32 v){u32 bits;std::memcpy(&bits,&v,4);tests::word(b,at,bits);};
        for(u32 i=0;i<9;++i)scalar(20+i*4,std::array<f32,9>{-10,0,-10,-10,0,10,10,0,-10}[i]);
        scalar(60,1); // Normal (0,1,0).
        // 20 + vertices(36) + normals(12) + edges(12) = 80.
        b[83]=1;b[85]=2;tests::word(b,96,attr);
        scalar(100,-10);scalar(104,-1);scalar(108,-10);
        scalar(112,20);scalar(116,2);scalar(120,20);b[125]=1;
        return tests::bytes(std::move(b));
    };
    auto &sat=game.sat();auto &eat=game.eat();sat.configure(2);eat.configure(1);
    auto h=sat.create(fixture(SAT_ATTR_PL_NOHIT));Vec pos{-2,.25f,-2},normal;
    c.test(!sat.pushSphere({-2,2,-2},pos,1,normal,SAT_TYPE_PL) && pos.y==.25f,"original SAT player attribute rejection");
    c.test(sat.pushSphere({-2,2,-2},pos,1,normal,SAT_TYPE_EM|SAT_CK_FLOOR) && pos.y==1 && normal.y==1,"original sphere face push-out and normal");
    pos={-2,.25f,-2};c.test(!sat.pushSphere({-2,2,-2},pos,1,normal,0,SAT_ATTR_PL_NOHIT),"original sphere attribute mask");
    c.test(!sat.pushSphere({-2,-2,-2},pos,1,normal),"original back-face rejection");
    sat.enable(h,false);c.test(!sat.pushSphere({-2,2,-2},pos,1,normal),"disabled collision work excluded");sat.enable(h,true);
    pos={-2,.25f,-10.5f};c.test(sat.pushSphere({-2,2,-10.5f},pos,1,normal) &&
        std::fabs(std::sqrt(pos.y*pos.y+(pos.z+10)*(pos.z+10))-.99f)<1e-5f,"original edge response retains 0.99 radius");
    pos={-2,.25f,-10.5f};c.test(!sat.pushSphere({-2,2,-10.5f},pos,1,normal,SAT_EDGE_CANCEL),"original edge cancellation flag");
    auto e=eat.create(fixture(EAT_ATTR_SMALL_NOHIT));pos={-2,.25f,-2};
    c.test(!eat.pushSphere({-2,2,-2},pos,1,normal,SAT_TYPE_SMALL),"original EAT filter selected without prior line query");
    c.test(eat.pushSphere({-2,2,-2},pos,1,normal,0) && pos.y==1,"original EAT sphere allowed with other type");
    c.rejects([&]{sat.pushSphere({},pos,0,normal);},"invalid native radius rejected");
    // Original complete scenery-check bodies on a numeric adapter fixture.
    // These are not cPlayer::startUp or player simulation ticks.
    auto actor=std::make_shared<ActorModelCpu>();auto linked=std::make_shared<ActorModelCpu>();
    DamagePart damagePart;Vec from{3,4,5};actor->dmg.set(4,3,2,&from,6,&damagePart);
    c.test(actor->dmg.m_Flag==5 && actor->dmg.m_Wep==2 && actor->dmg.m_pDamageYarare==&damagePart && actor->dmg.m_PosFrom.y==4,
           "original damage record retains typed native part pointer");
    for(int n=0;n<3;++n)actor->dmg.move();c.test(!actor->dmg.m_Flag && !actor->dmg.m_Timer,"original damage timer expires");
    actor->dmg.set(7,0x83);actor->dmg.move();c.test(actor->dmg.m_Timer==0x83 && actor->dmg.m_Flag==7,"original damage hold bit preserves timer");
    actor->dmg.clear();c.test(!actor->dmg.m_Timer && !actor->dmg.m_Flag && actor->dmg.m_pDamageYarare==&damagePart,"original damage clear preserves non-state fields");
    actor->atari.init(0,0,0,1,1,1,2,0,0,0);
    linked->atari.init(0,0,0,1,1,1,2,0,0,0);
    actor->pos=actor->pos_old={-2,2,-2};linked->pos=linked->pos_old={-1,2,-2};
    actor->atari.m_pMod=linked.get();
    c.rejects([&]{sat.checkActor(actor);},"unowned dynamic actor link rejected before dereference");
    c.test(sat.checkActor(actor,0,false,{linked})==0 && std::fabs(actor->pos.y)<.02f && std::fabs(linked->pos.y)<.02f,
           "original sphere actor floor snap including linked model");
    c.test(actor->pFloor_norm==&actor->nativeFloorNormal && actor->pFloor_norm->y==1,
           "floor normal copied to model lifetime");
    actor->atari.m_pMod=nullptr;actor->pos=actor->pos_old={-2,2,-2};
    c.test(sat.checkActor(actor,0,true)==0 && actor->pos.y==2,"original airborne actor preserves height");
    actor->atari.offSca();c.test(sat.checkActor(actor)==0 && actor->pos.y==2,"original actor scenario collision disabled");actor->atari.onSca();
    actor->atari.m_pList=&actor->atari;
    c.rejects([&]{sat.checkActor(actor);},"cyclic native collision info rejected");actor->atari.m_pList=nullptr;
    actor->atari.set(5,9,4);for(int n=0;n<4;++n)actor->atari.move();
    c.test(actor->atari.m_radius==5 && actor->atari.m_radius2==9 && !actor->atari.m_hokan,"original radius interpolation over four ticks");
    actor->atari.setPriority(3);c.test((actor->atari.m_flag&0x18)==0x18,"original actor push priority mask");
    actor->atari.m_parts_no=1;c.rejects([&]{sat.checkActor(actor);},"missing original part owner rejected");actor->atari.m_parts_no=0;
    sat.destroy(h);eat.destroy(e);sat.clear();eat.clear();
    c.test(actor->pFloor_norm->y==1 && linked->pFloor_norm->y==1,"SAT release does not invalidate actor normal");
    c.rejects([&]{sat.snapshot(h);},"sphere work lifetime invalidates references");
}
// Explicit isolated consumer-order fixture. No actor/event/trigger implementation.
struct ConsumerOrder final : SharedScenarioConsumers {
    std::vector<std::string> calls;
    void SceAtSetSaveItem() override { calls.push_back("saved-items"); }
    void SceExecInitCondition() override { calls.push_back("init-conditions"); }
    void SceExecCheckCondition() override { calls.push_back("check-conditions"); }
    void SubCharInit(int, Vec *, float) override { calls.push_back("partner-init"); }
    void SubCharCtrl(int, int) override { calls.push_back("partner-control"); }
    void EmSetFromList() override { calls.push_back("enemy-list"); }
    void SceAtRoomSet() override { calls.push_back("areas"); }
    void SubMissionCheck() override { calls.push_back("missions"); }
    void SceAtCheckMoveScrAt() override { calls.push_back("moving-areas"); }
    int scenarioCheckEventCancel() override {
        calls.push_back("cancel-check");
        return 0;
    }
    void EventRun() override { calls.push_back("events"); }
    void MessageStageInit() override { calls.push_back("stage-messages"); }
};
std::shared_ptr<ImportedFiles> fixture(const std::string &directory) {
    auto root = std::filesystem::u8path(directory);
    std::filesystem::create_directories(root / "blobs");
    std::vector<u8> bytes(0x1fe0, 0x5a); // Bounded short-file adapter test, no enemy positions.
    auto hash = sha256(bytes.data(), bytes.size());
    std::ofstream blob(root / "blobs" / hash, std::ios::binary);
    blob.write(reinterpret_cast<const char *>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
    blob.close();
    std::ofstream catalog(root / "catalog.tsv");
    catalog << "RE4-IMPORT-CATALOG-1\n";
    for (int n = 0; n <= 10; ++n)
        catalog << "0\t" << n + 1 << "\t8160\tetc/emleon" << (n < 10 ? "0" : "") << n << ".esl\t"
                << hash << '\n';
    catalog.close();
    return std::make_shared<ImportedFiles>(directory, (root / "catalog.tsv").u8string());
}
void inputTests(Checks &c) {
    ControllerInput input;
    PadRuntime pad(input);
    pad.tick();
    c.test(pad.joy().err == -1, "disconnect original PAD error");
    input.connect(9, 0);
    pad.tick();
    c.test(pad.joy().on == 0 && pad.key().on == 0, "neutral original pad");
    const std::pair<int, u16> mapping[] = {
        {19, JOY_UP}, {20, JOY_DOWN}, {21, JOY_LEFT}, {22, JOY_RIGHT}, {96, JOY_A},     {97, JOY_B},
        {99, JOY_X},  {100, JOY_Y},   {102, JOY_L},   {103, JOY_Z},    {108, JOY_START}};
    for (const auto &[key, bit] : mapping) {
        c.test(input.button(9, key, true), "mapped Android key");
        pad.tick();
        c.test(pad.joy().on == bit && pad.joy().trg == bit && pad.joy().rep == bit,
               "original press/trg/repeat");
        c.test((pad.key().on >> 32) == 0, "preserve original u32 KEY cursor");
        c.test(input.button(9, key, false), "mapped release");
        pad.tick();
        c.test(pad.joy().on == 0 && pad.joy().rel == bit && pad.key().rel != 0, "original release");
    }
    c.test(!input.button(9, 109, true) && !input.button(9, 106, true), "unmapped GC-less buttons");
    input.button(9, 96, true);
    pad.tick();
    c.test((pad.key().on & 0x80000000U) != 0, "original KEY_A mapping");
    for (int i = 0; i < 11; ++i) {
        pad.tick();
        c.test(pad.joy().rep == 0, "original repeat delay");
    }
    pad.tick();
    c.test(pad.joy().rep == JOY_A, "24-retrace repeat threshold");
    input.button(9, 96, false);
    pad.tick();
    input.axes(9, 0.15f, 0, 0, 0, 0, 0, 30.0f / 255, 0);
    pad.tick();
    c.test(pad.joy().stickX == 0 && pad.joy().triggerLeft == 0, "SDK deadzones unchanged");
    input.axes(9, 1, -1, -1, 1, 0, 0, 1, 1);
    pad.tick();
    c.test(pad.joy().stickX == 40 && pad.joy().stickY == 40, "SDK octagonal main-stick clamp");
    c.test(pad.joy().substickX == -31 && pad.joy().substickY == -31, "SDK octagonal C-stick clamp");
    c.test(pad.key().triggerLeft == 150 && pad.key().triggerRight == 150, "SDK trigger clamp");
    c.test((pad.joy().on & (JOY_L | JOY_R)) == (JOY_L | JOY_R), "analog shoulder gates");
    input.clearHeld();
    pad.tick();
    input.axes(9, 0, 0, 0, 0, -1, 0, 0, 0);
    input.button(9, 21, true);
    pad.tick();
    c.test(pad.joy().on == JOY_LEFT && pad.joy().trg == JOY_LEFT, "hat + key merge once");
    input.axes(9, 0, 0, 0, 0, 0, 0, 0, 0);
    pad.tick();
    c.test(pad.joy().on == JOY_LEFT && pad.joy().trg == 0, "held dpad key survives hat release");
    input.button(9, 22, true);
    pad.tick();
    c.test((pad.key().on & ((1U << 2) | (1U << 3))) == 0,
           "original opposite direction cancellation");
    input.clearHeld();
    pad.tick();
    input.button(9, 104, true);
    input.axes(9, 0, 0, 0, 0, 0, 0, 0.2f, 0);
    pad.tick();
    c.test(pad.joy().triggerLeft == 21, "digital trigger event cannot override real analog axis");
    input.connect(9, 0, false, false);
    input.button(9, 104, true);
    pad.tick();
    c.test(pad.joy().triggerLeft == 150, "digital fallback for controller without analog trigger");
    pad.stopKeys(0);
    pad.tick();
    c.test(pad.key().on == 0 && pad.key().triggerLeft == 0, "original key stop");
    input.disconnect(9);
    pad.tick();
    c.test(pad.joy().on == 0 && pad.key().on == 0, "disconnect clears held");
    input.connect(10, 0);
    pad.state().Stop_flg = 0;
    pad.tick();
    c.test(!input.button(9, 96, true), "stale device events rejected");
    c.rejects([&] { input.axes(10, std::numeric_limits<float>::quiet_NaN(), 0, 0, 0, 0, 0, 0, 0); },
              "reject NaN input");
    c.rejects([&] { input.connect(1, 4); }, "reject fifth gamepad");
    pad.clear();
    pad.tick();
    c.test(pad.key().on == 0, "repeated typed input reset");
}
void heapTests(Checks &c) {
    GameplayHeaps heaps(4096);
    auto system = heaps.allocate(65, 13, true);
    c.test(reinterpret_cast<std::uintptr_t>(system.data) % 32 == 0, "native allocation alignment");
    c.test(static_cast<u8 *>(system.data)[64] == 0 && heaps.liveBytes() == 96,
           "bounded clear/alignment accounting");
    c.test(heaps.replace(0, 1) && heaps.setCurrent(1), "system to game tail handoff");
    auto game = heaps.allocate(128);
    std::memset(game.data, 0x73, 128);
    c.test(heaps.replace(1, 2) && heaps.setCurrent(2), "game to stage tail handoff");
    auto stage = heaps.allocate(256);
    c.test(heaps.replace(2, 3) && heaps.setCurrent(3), "stage to module tail handoff");
    auto module = heaps.allocate(128);
    heaps.suspend(3);
    c.test(!heaps.setCurrent(3), "suspended heap cannot become current");
    c.rejects([&] { heaps.allocate(1); }, "suspended allocation rejected");
    heaps.resume(3);
    c.test(heaps.setCurrent(3), "heap resume");
    c.rejects([&] { heaps.allocate(SIZE_MAX); }, "overflow allocation");
    c.rejects([&] { heaps.allocate(4096); }, "bounded allocation");
    c.rejects([&] { heaps.setCurrent(13); }, "heap 13 is allocation selector only");
    c.test(!heaps.replace(1, 2), "live target lease cannot be overwritten");
    heaps.reset();
    c.test(heaps.liveBytes() == 0, "new arena excludes retired leases");
    c.test(static_cast<u8 *>(game.data)[127] == 0x73, "retired arena lease remains valid");
    auto fresh = heaps.allocate(128);
    c.test(fresh.data != game.data, "fresh arena does not alias pinned old data");
}
} // namespace

std::string verifyGameRuntime(const std::string &directory,std::shared_ptr<GraphicsPresenter> presenter) {
    Checks c;
    inputTests(c);
    heapTests(c);
    // Independent analytic equal-bone oracle for the complete original IK solve.
    // This isolated synthetic fixture is not a player pose or an animation asset.
    for(float distance:{80.f,120.f,150.f,200.f,250.f}) {
        ModelCpu model;ModelPart root,joint,effector;
        model.nParts=3;model.pList=&root;root.pList=&joint;joint.pList=&effector;
        root.pParent=&model;joint.pParent=&root;effector.pParent=&joint;
        for(auto *p:{&root,&joint,&effector})for(int r=0;r<3;++r){for(int col=0;col<4;++col)p->lt_inv_mat[r][col]=p->mat[r][col]=(r==col)?1.f:0.f;}
        joint.lt_inv_mat[0][3]=-100;effector.lt_inv_mat[0][3]=-200;joint.pos={100,0,0};
        u16 kind=0x0410;u8 number=0;ModelMotion motion{};motion.Joint_num=1;motion.pJoint_kind=&kind;motion.pJoint_no=&number;
        IKInit(&model,&motion);effector.world={distance,0,0};ikCalc(&root,&joint,&effector);
        const float x=distance<200?distance*.5f:100.f;
        const float side=distance<200?std::sqrt(10000.f-x*x):0.f;
        c.test(std::abs(joint.world.x-x)<.002f && std::abs(std::sqrt(joint.world.y*joint.world.y+joint.world.z*joint.world.z)-side)<.002f,"original IK solve vs independent equal-bone intersection");
        float endpoint[3];for(int r=0;r<3;++r)endpoint[r]=joint.mat[r][0]*100+joint.mat[r][3];
        c.test(std::abs(endpoint[0]-std::min(distance,200.f))<.002f && std::abs(endpoint[1])<.002f && std::abs(endpoint[2])<.002f,"original IK effector reaches/clamps analytic target");
    }
    ConsumerOrder consumers;
    ControllerInput input;
    input.connect(9, 0);
    u32 now = 500;
    GameRuntime game(fixture(directory), input, 0, [&] { return now; });
    game.graphics().attach(std::move(presenter));
    game.initialize();
    // Source-table checks, isolated original merchant consumer execution. The
    // stage/debug test inputs are restored before any scenario is dispatched.
    const auto originalState=game.state();auto &merchant=game.merchant();
    merchant.gameInit();auto bootMerchant=merchant.snapshot();
    c.test(bootMerchant.initialized && !bootMerchant.characterBound && bootMerchant.data.friendship==50,"original merchant boot state");
    for(const auto &entry:bootMerchant.data.stock.e)c.test(entry.id==0xffff && entry.num==0 && !entry.isNew,"original stock sentinel reset");
    merchant.secondRoundInit();auto clearedMerchant=merchant.snapshot();
    c.test(clearedMerchant.characterBound && clearedMerchant.data.stock.e[0].id==3 && clearedMerchant.data.stock.e[0].num==1 && clearedMerchant.data.stock.e[1].id==0x6d,"original second-round stock table");
    c.test(clearedMerchant.data.level.e[0].id==3 && clearedMerchant.data.level.e[0].lv[0]==6 && clearedMerchant.data.level.e[0].lv[3]==7,"original second-round tune table");
    game.state().room_id=0x300;game.state().Debug_flg[3]|=0x00800000;
    merchant.gameInit();auto debugMerchant=merchant.snapshot();
    c.test(std::count_if(std::begin(debugMerchant.data.stock.e),std::end(debugMerchant.data.stock.e),[](const auto &e){return e.id!=0xffff;})>10,"original stage1/2 debug stock aggregation");
    c.test(std::count_if(std::begin(debugMerchant.data.level.e),std::end(debugMerchant.data.level.e),[](const auto &e){return e.id!=0xffff;})>10,"original stage1/2 debug tune aggregation");
    game.state()=originalState;merchant.gameInit();
    c.test(merchant.snapshot().data.stock.e[0].id==0xffff && !merchant.snapshot().characterBound,"original reinit clears stock and typed references");
    merchant.clear();c.test(!merchant.snapshot().initialized,"merchant native owner retirement");
    c.rejects([&]{merchant.secondRoundInit();},"second round before original gameInit rejected");
    merchant.gameInit();game.initialize();
    c.test(!merchant.snapshot().initialized && merchant.trace().empty(),"GameRuntime reset retires merchant state and typed references");
    c.test(game.state().room_id == 0x120 && game.state().RoomNo_next == 0x120,
           "original boot room");
    c.test(game.state().debug_mode == 1 && game.state().debug_disp == -1 &&
               game.state().game_mode == 5,
           "original boot settings");
    c.test(game.state().pl_type == 0 && game.state().game_country == 1,
           "original Leon / English init");
    // Isolated execution of the complete original player reset and key consumer.
    // No player actor, synthetic movement, REL or weapon gameplay is installed.
    {
        auto saved=game.state(); auto &g=game.state(); auto &player=game.player();
        const u16 life[]={1200,600,1560,1680,2400,1860,1200};
        const u8 weapon[]={2,2,1,11,28,2,93};
        for(u8 type=0;type<7;++type) for(int flags=0;flags<8;++flags) {
            g=NativeGameState{}; g.pl_type=type;
            g.System_flg=(flags&1)?(0x80000000u>>SYS_OMAKE_ETC_GAME):0;
            if(flags&2) g.Debug_flg[DBG_START_ST2/32]|=0x80000000u>>(DBG_START_ST2%32);
            if(flags&4) g.Debug_flg[DBG_START_ST3/32]|=0x80000000u>>(DBG_START_ST3%32);
            g.Debug_flg[DBG_PL_LOCK_FOLLOW/32]|=0x80000000u>>(DBG_PL_LOCK_FOLLOW%32);
            g.weapon_no=93;g.weapon_type=9;g.weapon_lv_power=4;g.weapon_lv_speed=5;g.weapon_lv_blt=6;g.weapon_lv_reload=7;
            g.weapon_no_old=0;g.pl_flag=0xff;g.Status_flg[3]=0x12345678;
            player.gameInit();
            u16 expected=(flags&4)?1920:((flags&2)?1440:((type==0 && (flags&1))?1860:life[type]));
            c.test(g.pl_life==expected && g.pl_life_max==expected && g.ashley_life==600 && g.ashley_life_max==600,"original life and debug override order");
            c.test(g.weapon_no==weapon[type] && g.weapon_type==(type==6?9:(type==5?1:0)),"original character equipment reset/default preservation");
            c.test(type==6?(g.weapon_lv_power==4 && g.weapon_lv_speed==5 && g.weapon_lv_blt==6 && g.weapon_lv_reload==7):
                !(g.weapon_lv_power || g.weapon_lv_speed || g.weapon_lv_blt || g.weapon_lv_reload),"original upgrade reset/default preservation");
            c.test(g.pl_flag==1 && g.weapon_no_old==0xff && player.snapshot().oldWeaponId==0xff && player.snapshot().kaiou==0 && player.snapshot().weaponSlotEmpty,"original module release and player flags");
            c.test(!(g.Debug_flg[DBG_PL_LOCK_FOLLOW/32]&(0x80000000u>>(DBG_PL_LOCK_FOLLOW%32))) && g.Status_flg[3]==0x12345678,"original precise flag mutation");
        }
        auto baseline=game.heaps().liveBytes();
        for(bool debug:{false,true}) {
            g.weapon_no_old=2;auto *p=player.allocateWeaponData(4096,debug);std::memset(p,0x5a,4096);
            c.test(game.heaps().liveBytes()>baseline && !player.snapshot().weaponSlotEmpty,"owned native weapon allocation adapter");
            player.gameInit();c.test(game.heaps().liveBytes()==baseline && player.snapshot().weaponSlotEmpty,"original InitModule frees normal/debug owned allocation");
            player.gameInit();c.test(game.heaps().liveBytes()==baseline,"repeated original ReleaseWepData idempotence");
        }
        g.weapon_no_old=0xff;player.allocateWeaponData(64);player.gameInit();
        c.test(!player.snapshot().weaponSlotEmpty,"original FF weapon sentinel must not release slot");
        g.weapon_no_old=1;player.gameInit();c.test(player.snapshot().weaponSlotEmpty,"original later weapon release");
        // Real native PadRead state is tested elsewhere. This isolated KEY models
        // its output and runs the actual cPlayer consumer without a second mapping.
        GameplayHeaps h;EngineConsumers models;NativeGameState state;KEY key{};u8 layout=0;
        ObjectRuntime objects(h,models,game.loader().scheduler());
        EffectRuntime effects(h,game.loader().scheduler(),objects);
        PlayerRuntime keys(state,key,layout,h,models,game.loader().scheduler(),objects,effects);
        key.on=2;key.trg=0x40000000;keys.keyConfig();c.test(key.trg==(0x40000000|0x100),"original B trigger / held action combination");
        key.trg=0;keys.keyConfig();c.test(key.trg==0x100,"original B latch survives one frame");
        key.trg=0;keys.keyConfig();c.test(key.trg==0,"original B latch expires");
        layout=1;key.trg=0x40000000;keys.keyConfig();c.test(key.trg==(0x40000000|0x100),"original alternate layout shares TypeA");
        layout=2;key.trg=0x100;keys.keyConfig();c.test(key.trg==0x100,"original unknown layout does not mutate KEY");
        player.clear();g=saved;
    }
    game.state().play_time = 3661;
    game.initGameTime();
    u32 h = 0, m = 0, s = 0;
    c.test(game.gameTime(&h, &m, &s) == 3661 && h == 1 && m == 1 && s == 1,
           "original game time split");
    game.doorFlagInit();
    for (int bit : {0x96, 0x98, 0x9a, 0x9b, 0x9d, 0xa3, 0x72, 0x7a, 0x7f, 0x80, 0x84, 0x89})
        c.test((game.state().Scenario_flg[bit / 32] & (0x80000000U >> (bit % 32))) != 0,
               "original door flags ABI safe");
    auto &scheduler = game.loader().scheduler();
    auto old = game.sceExec(18, [] { original::SceSleep(20); });
    c.rejects([&] { game.scenarioRoomInit(); },
              "missing original saved-item consumer explicit failure");
    c.test(old && !scheduler.state(*old), "original room init kills scenario tasks");
    game.bindConsumers(&consumers);
    auto activation = game.queueModule(ModuleOperation::Activate, 78); // Real st1_2 / r10d.
    c.test(activation->status == OperationStatus::Pending, "module request deferred");
    game.tick();
    c.test(activation->status == OperationStatus::Complete, "module activation at frame boundary");
    game.state().room_id = 0x10d;
    game.scenarioRoomInit();
    c.test(consumers.calls ==
               std::vector<std::string>{"saved-items", "init-conditions", "enemy-list", "areas"},
           "isolated original room-init consumer order");
    c.test(game.modules().roomWorkLease(0x10d).size == 1, "actual original R10dInit allocation");
    c.test(game.heaps().liveBytes() >= 32, "original room work uses shared gameplay heap");
    consumers.calls.clear();
    std::vector<int> taskTrace;
    auto task = game.sceExec(18, [&] {
        int local = 7;
        taskTrace.push_back(local);
        original::SceSleep(1);
        local += 5;
        taskTrace.push_back(local);
    });
    game.scenarioMove();
    c.test(taskTrace == std::vector<int>{7}, "original scenario pass enters stackful task");
    game.pause();
    now += 500;
    game.tick();
    c.test(taskTrace.size() == 1, "pause retains task stack");
    c.test(game.gameTime(nullptr, nullptr, nullptr) == 3661,
           "paused wall time excluded from original play time");
    auto frames = game.state().Frame_cnt;
    c.test(game.displayFrame(900000000000LL) == 0 && game.state().Frame_cnt == frames,
           "paused VI ignores wall time");
    game.resume();
    game.scenarioMove();
    c.test(taskTrace == std::vector<int>{7, 12} && task && !scheduler.state(*task),
           "scenario resumes exact local state");
    c.test(consumers.calls == std::vector<std::string>{"check-conditions", "cancel-check", "events",
                                                       "missions", "moving-areas",
                                                       "check-conditions", "cancel-check", "events",
                                                       "missions", "moving-areas"},
           "isolated original scenario-move order");
    auto previous = game.modules().room(0x10d)->main;
    bool transitioned = false;
    auto main = scheduler.exec(2, [&] {
        game.awaitModule(ModuleOperation::Stop, 78);
        game.awaitModule(ModuleOperation::Restart, 78);
        transitioned = true;
        scheduler.exit();
    });
    for (int i = 0; i < 8 && !transitioned; ++i)
        game.tick();
    c.test(main && transitioned, "stackful frame-boundary stop/restart");
    game.modules().invoke(previous);
    auto ticket = game.queueModule(ModuleOperation::ResetRoom, 0x10d);
    game.tick();
    c.test(ticket->status == OperationStatus::Complete, "room reset at boundary");
    c.rejects([&] { game.modules().invoke(previous); }, "stale room callback rejected");
    std::shared_ptr<FrameOperation> cancelled;
    auto doomed = scheduler.exec(1, [&] {
        cancelled = game.queueModule(ModuleOperation::Deactivate, 78);
        scheduler.sleep(2);
        scheduler.exit();
    });
    game.tick();
    c.test(doomed && scheduler.kill(*doomed), "cancel requesting task");
    game.tick();
    c.test(cancelled->status == OperationStatus::Cancelled &&
               game.modules().info(78).phase == ModulePhase::Active,
           "cancelled requester cannot mutate module");
    game.initialize();
    c.test(game.state().Frame_cnt == 0 && game.state().room_id == 0x120 &&
               scheduler.allocatedStacks() == 0,
           "repeated native/original boot reset");
    for (int hz : {60, 90, 120}) {
        game.initialize();
        game.displayFrame(0);
        for (int i = 1; i <= hz; ++i)
            game.displayFrame(1000000000LL * i / hz);
        c.test(game.state().Frame_cnt == 30, "30 Hz independent of Android display rate");
    }
    game.initialize();
    game.startOriginalGameTask();
    std::string gameFailure;
    try {
        game.tick();
    } catch (const MissingGameConsumer &failure) {
        gameFailure = failure.what();
    }
    c.test(gameFailure == "Original consumer unavailable: cMes.gameInit()",
           "actual GameTask passes original GX clear register state and reaches Message gameInit");
    c.test(std::find(game.trace().begin(),game.trace().end(),"ClothInit:original")!=game.trace().end(),
           "actual original ClothInit executes before GX clear");
    c.test(game.state().Rno0 == 0 && game.state().Rno1 == 0 && game.state().Rno2 == 0 &&
               game.state().Rno3 == 0,
           "actual original GameTask initial state executed");
    game.initialize();
    c.test(game.enemyListNumber(0x120) == 0 && game.enemyListNumber(0x100) == 0 &&
               game.enemyListNumber(0x315) == 7,
           "original universal ESL selection");
    c.test(game.enemyListName(0) == "etc/emleon00.esl" && game.enemyListName(11) == "...no string",
           "original ESL names and bounds");
    c.test(!game.modules().room(0x120), "r120 callback is honestly unavailable");
    auto start = game.queueModule(ModuleOperation::Activate, 74);
    game.tick();
    c.test(start->status == OperationStatus::Complete,
           "original st1_0 registration component activated");
    game.scenarioRoomInit();
    c.test(game.texRender().count()==1,
           "actual r120 CPU initialization with explicitly isolated scenario consumers");
    game.initialize();
    consumers.calls.clear();
    game.state().em_list_no = -1;
    game.state().Em_list[8191] = 0x71;
    bool stageDone = false;
    auto stageTask = scheduler.exec(1, [&] {
        game.stageSet();
        stageDone = true;
        scheduler.exit();
    });
    for (int i = 0; i < 5000 && !stageDone; ++i) {
        game.tick();
        std::this_thread::yield();
    }
    c.test(stageTask && stageDone, "original StageSet and readEmList stackful flow");
    c.test(consumers.calls == std::vector<std::string>{"stage-messages"},
           "isolated message consumer in original stage order");
    c.test(game.state().em_list_no == 0 && game.state().Em_list[0] == 0x5a &&
               game.state().Em_list[8159] == 0x5a && game.state().Em_list[8191] == 0x71,
           "original ESL short-read preserves untouched destination tail");
    game.state().room_id_prev = game.state().room_id;
    game.state().room_id = 0x328;
    stageDone = false;
    stageTask = scheduler.exec(1, [&] {
        game.stageSet();
        stageDone = true;
        scheduler.exit();
    });
    for (int i = 0; i < 5000 && !stageDone; ++i) {
        game.tick();
        std::this_thread::yield();
    }
    c.test(stageTask && stageDone && game.modules().info(74).phase == ModulePhase::Inactive &&
               game.modules().info(101).phase == ModulePhase::Active,
           "original StageSet cross-stage relink at frame boundaries");
    game.scenarioRoomInit();
    c.test(game.modules().roomWorkLease(0x328).size == 1,
           "actual R328Init after original cross-stage flow with isolated consumers");
    auto pinnedWork = game.modules().roomWorkLease(0x328);
    game.state().room_id_prev = game.state().room_id;
    game.state().room_id = 0x10d;
    auto blockedSwitch = scheduler.exec(1, [&] {
        game.stageSet();
        scheduler.exit();
    });
    bool blocked = false;
    for (int i = 0; i < 100 && !blocked; ++i) {
        try {
            game.tick();
        } catch (const std::logic_error &failure) {
            blocked = std::string(failure.what()) ==
                      "MemReplaceHeap: native live-resource boundary prevents handoff";
            if (!blocked)
                throw;
        }
    }
    c.test(blockedSwitch && blocked && *pinnedWork.data == 0,
           "initialized room stage-switch stops explicitly before unsafe heap reuse");
    game.initialize();
    c.test(pinnedWork.data && *pinnedWork.data == 0 &&
               game.modules().info(101).phase == ModulePhase::Inactive,
           "external original room-work lease survives reset while callback invalidates");
    // Kill a sleeping original ESL task repeatedly. More than the 16 DVD queue
    // entries are exercised; abandoned requests must not accumulate.
    for (int n = 0; n < 20; ++n) {
        game.state().em_list_no = -1;
        auto reader = scheduler.exec(1, [&] {
            game.readEnemyList(0); // Original proc=0 => asynchronous mode 0x10.
            scheduler.exit();
        });
        game.tick();
        c.test(reader && scheduler.kill(*reader), "cancel sleeping original readEmList task");
        for (int i = 0; i < 4; ++i) {
            game.tick();
            std::this_thread::yield();
        }
    }
    bool recovered = false;
    game.state().em_list_no = -1;
    auto reader = scheduler.exec(1, [&] {
        game.readEnemyList(0);
        recovered = true;
        scheduler.exit();
    });
    for (int i = 0; i < 5000 && !recovered; ++i) {
        game.tick();
        std::this_thread::yield();
    }
    c.test(reader && recovered && game.state().Em_list[8159] == 0x5a,
           "original readEmList completes after repeated cancellation without queue leak");
    game.initialize();
    c.test(scheduler.allocatedStacks() == 0, "all stack owners released on reset");
    tests::consumerTests(c, game);
    tests::modelInitializationTests(c, game);
    sphereTests(c,game);
    tests::objectTests(c,game);
    tests::lightParentTests(c, game);
    tests::texRenderTests(c, game);
    tests::openingTests(c, game);
    tests::retirementTests(c, game);
    tests::automaticTransitionTests(c, game);
    tests::callbackStackTests(c, game);
    c.test(game.graphics().clear().depth<=0xffffff,"original clear depth stays a 24-bit GX value");
    game.graphics().setCopyClear({18,52,86,120},0x123456);
    c.test(game.graphics().clear().rgba==std::array<u8,4>{18,52,86,120} && game.graphics().clear().depth==0x123456,
           "actual original GXSetCopyClear packs and restores RGBA/depth registers");
    c.rejects([&]{game.graphics().setCopyClear({0,0,0,0},0x1000000);},"original GX clear depth guard");
    std::ostringstream out;
    out << "{\"pointer_bits\":" << sizeof(void *) * 8 << ",\"checks\":" << c.count
        << ",\"failed\":0,"
           "\"kind\":\"original_scalar_pad_scenario_bodies_and_isolated_consumer_order_tests\","
           "\"trace\":[\"systemWorkInit:r120,Leon,English,mode5\",\"pad:original-clamp,trg,rel,"
           "repeat,u32-key-cursor\","
           "\"heaps:bounded,lease-pinned,tail-handoff\",\"scenario:original-order,stack-sleep,"
           "pause,resume\","
           "\"modules:deferred,stop,restart,stale,cancel\",\"VI:60,90,120=>30\","
           "\"GameTask:original-gameDebug-gameInit-ClothInit-GXSetCopyClear,blocked-cMes.gameInit\","
        << (game.graphics().bound()?"\"r120:original-init-TexRenderInit,opening-SceSleep-original-VI,blocked-Sofdec,12-cleanups\",":
                                   "\"r120:original-init-TexRenderInit,opening-SceSleep,blocked-VI,12-cleanups\",")
        <<
           "\"models:original-modelInit-joints-matrices-bounds,pool-reuse-rollback\","
           "\"H1:original-Hokan-slerp-scale-cancel,cMot3-owned-blend,typed-cEm-model-root\","
           "\"H1:original-SAT-EAT-sphere-face-edge-mask-lifetime\","
           "\"H1:original-ObjMgr-front-back-indexed-deferred-destruction,typed-weapon-constructor\","
           "\"H1:original-Esp-Espgen-Efm-delete-bodies,numeric-lifecycle-fixtures\","
           "\"consumers:original-model-joints,message-address,camera-state,event-callback\","
           "\"retirement:20-original-room-entries,pending-borrowers,frame-boundary\","
           "\"transitions:20-initialized-cross-stage,sleeping-borrowers,same-module-reentry\","
           "\"event-stack:original-ExeFunc,isolated-callback,sleep,stop-restore,cancel-safe-"
           "ledger\"]}";
    return out.str();
}
} // namespace re4::port
