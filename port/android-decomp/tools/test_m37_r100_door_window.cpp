// Execute the literal original SceSleep task bodies on separate host stacks.
// The native state machine is checked against those suspended source stacks,
// actual r100 ETS/AEV/SAT, and production door collision/window availability.
#include "room_interactions.h"
#include "r100_scenario.h"
#include "leon_preview.h"
#include <ucontext.h>
#include <algorithm>
#include <cmath>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <vector>
namespace {
unsigned checks;
void require(bool value,const char *why) {++checks;if(!value)throw std::runtime_error(why);}
std::vector<std::uint8_t> sub(const re4::RoomArchive &room,const char *tag) {
  const auto *entry=room.find(tag);require(entry,"Actual r100 subfile exists");
  return room.copy(entry-room.entries().data());
}
re4::CollisionVec world(const re4::RoomObjectPlacement &p,float x,float y,float z) {
  re4::SmdPlacement pose;pose.position=p.position;pose.rotation=p.rotation;pose.scale={1,1,1};
  const auto v=re4::TransformPoint(re4::PlacementMatrix(pose),{x,y,z});return {v[0],v[1],v[2]};
}
// Host substitutes only the GC owners and cooperative stack scheduler.
constexpr unsigned G_ROOM_ID=0x100;
constexpr float PI=3.1415927f,PI2=6.2831855f;
struct Globals {std::uint32_t Room_flg[4]{};} globals,*pG=&globals;
re4::RoomSaveState *sourceSave;
int RsfCheck(unsigned,unsigned bit) {return sourceSave->room.test(bit);}
void RsfSet(unsigned,unsigned bit) {sourceSave->room.set(bit);}
struct cEmGanado {bool found{};unsigned flag{};int ckFindPL(){return found;}};
cEmGanado actors[3],missing,*errEm=&missing;
struct Work {cEmGanado *ems[3]{&actors[0],&actors[1],&actors[2]};} work,*W=&work;
struct cEmWindow {
  std::uint32_t flags{};
  void SetEnableFence(int enabled,int kind) {
    if(kind==0 || kind==1) {if(enabled)flags&=~0x40000000u;else flags|=0x40000000u;}
    if(kind==0 || kind==2) {if(enabled)flags&=~0x20000000u;else flags|=0x20000000u;}
  }
} windows[2];
float Muku2(float source,float target,float add) {
  float d=target-source;
  if(d<0)d+=PI2;
  if(d<PI) {if(add>d)add=d;}
  else {d-=PI2;if(add>-d)add=d;else add=-add;}
  return add;
}
float limit(float angle) {while(angle>=PI)angle-=PI2;while(angle<-PI)angle+=PI2;return angle;}
struct cEmDoor {
  std::array<float,3> pos{};
  float angle{},base{};
  unsigned r1{},r2{},r3{},flag{},Be_flg{},Timer{};
  void setOpenLock(int type) {r1=6;r2=0;r3=type;flag|=0x20000000;Be_flg|=1;}
  void setCloseLock() {r1=7;r2=0;r3=0;flag&=~0x20000000u;Be_flg|=1;}
  void setNormal() {Be_flg&=~1u;}
  void move() {
    if(r1==6) {
      if(r2==0) {flag|=0x30000000;Timer=20;angle=base+(r3?-PI/2:PI/2);++r2;}
      if(!(Be_flg&1))r1=r2=r3=0;
    } else if(r1==7) {
      if(r2==0) {flag&=~0x30000000u;Timer=20;++r2;}
      angle+=Muku2(angle,base,PI)*.3f;angle=limit(angle);
      if(Timer)--Timer;
      else {angle=base;if(!(Be_flg&1))r1=r2=r3=0;}
    }
  }
} door;
bool messageEnabled;
unsigned voiceCount;
int getRoomEtcDoor(unsigned number,cEmDoor **out,int) {if(number!=2)return 0;*out=&door;return 1;}
int getRoomEtcWindow(unsigned number,cEmWindow **out,int) {if(number>1)return 0;*out=&windows[number];return 1;}
void SceAtSetEnable(unsigned number,int enabled) {if(number!=0xb)std::abort();messageEnabled=enabled;}
void SndCall(unsigned block,unsigned number,const std::array<float,3> *position,int,int,int) {
  if(block!=6 || number!=0x28 || position!=&door.pos)std::abort();
  ++voiceCount;
}
struct Fiber {
  ucontext_t context{},caller{};
  std::vector<char> stack=std::vector<char>(128*1024);
  void (*function)();bool done{};
  static Fiber *current;
  explicit Fiber(void (*fn)()):function(fn) {
    getcontext(&context);context.uc_stack.ss_sp=stack.data();context.uc_stack.ss_size=stack.size();
    context.uc_link=nullptr;makecontext(&context,&Fiber::start,0);
  }
  static void start() {auto *self=current;self->function();self->done=true;self->yield();std::abort();}
  void yield() {swapcontext(&context,&caller);}
  void resume() {if(done)return;current=this;swapcontext(&caller,&context);}
};
Fiber *Fiber::current;
void SceSleep(unsigned ticks) {if(ticks!=1)std::abort();Fiber::current->yield();}
#include "oracles/r100_door_window_original.inc"
struct Input {
  bool rsf3{},rsf10{},rsf4{},inside=true,latched{};
  std::array<bool,3> found{};
};
}
int main(int argc,char **argv) {
  try {
    require(argc==4,"Usage: m37_r100_door_window_check R100 PL00 WEP01");
    const auto room=re4::RoomArchive::Load(argv[1]);
    const auto placements=re4::ReadRoomObjectPlacements(sub(room,"ETS"));
    const re4::Sat sat(sub(room,"SAT"));const re4::SatCollision collision(sat);
    const re4::R100Scenario areas(sub(room,"AEV"));
    re4::LeonPreview player(argv[2],argv[3]);re4::WalkProbe probe(collision);
    require(probe.reset(),"Actual r100 start floor validates the player");
    // Different source resumes, saved flags, external area flag and each of
    // the three independent original Ganado detection branches.
    for(unsigned test=0;test<14;++test) {
      re4::RoomSaveState save,oracleSave;
      if(test>=10)save.room.set(3);
      if(test==12 || test==13)save.room.set(10);
      if(test==13)save.room.set(4);
      oracleSave=save;sourceSave=&oracleSave;
      globals={};door={};windows[0]={};windows[1]={};
      for(auto &actor:actors)actor={};
      voiceCount=0;messageEnabled=false;
      re4::RoomInteractions native(0x100,room,placements,argv[2],save);
      door.base=native.doors()[0].placement.rotation[1];door.angle=door.base;
      door.pos=native.doors()[0].placement.position;
      probe.setScriptPosition(world(native.doors()[0].placement,-650,0,-650));
      player.setPlacement(probe.position(),door.base);
      Fiber windowTask(r100_WindowBreakCk),doorTask(r100_DoorCk);
      re4::SourceFlags<4> flags;
      unsigned totalNativeVoices=0;
      for(unsigned tick=0;tick<180;++tick) {
        Input input;
        input.rsf3=tick>=7;
        input.rsf10=tick>=40;
        input.inside=tick<120;
        if(test<3)input.found[test]=tick>=42;
        if(test==3)input.latched=tick==20;
        if(test==4)input.latched=tick==7; // cleared at initial RSF3 resume
        if(test==5) {input.latched=tick==7 || tick==8;input.rsf4=tick>=29;}
        if(test==6) {input.latched=tick==19;input.rsf4=tick>=39;}
        if(test==7) {input.latched=tick==19;input.rsf4=tick>=40;}
        if(test==8) {input.latched=tick==19;input.rsf4=tick>=41;}
        if(test==9) {input.rsf10=false;input.latched=tick==20;input.rsf4=tick>=79;}
        if(test==10) {input.latched=tick==0;input.found[0]=tick>=44;}
        if(test==11) {input.rsf3=false;input.rsf10=false;input.inside=true;}
        if(test==12) {input.inside=tick!=0;input.found[0]=true;}
        if(test==13)input.inside=false;
        if(input.rsf3) {save.room.set(3);oracleSave.room.set(3);}
        if(input.rsf10) {save.room.set(10);oracleSave.room.set(10);}
        if(input.rsf4) {save.room.set(4);oracleSave.room.set(4);}
        flags.clearFrameWords();globals.Room_flg[2]=globals.Room_flg[3]=0;
        flags.set(64,input.inside);if(input.inside)globals.Room_flg[2]|=0x80000000;
        if(input.latched) {flags.set(0);globals.Room_flg[0]|=0x80000000;}
        for(unsigned i=0;i<3;++i)actors[i].found=input.found[i];
        const bool wasReleased=oracleSave.room.test(4);
        windowTask.resume();doorTask.resume();door.move();
        const bool released=native.tickR100Scripts(save,flags,input.found);
        native.tick(player,probe);
        require(released==(!wasReleased && oracleSave.room.test(4)),"Source WindowBreakCk release resumes exactly once");
        require(save.room.words()==oracleSave.room.words(),"All original RSF words match suspended source tasks");
        require(flags.words()[0]==globals.Room_flg[0],"Door task clears and preserves the exact latched source bit");
        require(native.doors()[0].scriptLocked==bool(door.Be_flg&1),"Original Be_flg script lock survives every source yield");
        require(std::abs((native.doors()[0].placement.rotation[1]+native.doors()[0].yaw)-door.angle)<.000002,
            "Native locked door movement matches the source float angle at every yield");
        require(native.doors()[0].timer==door.Timer,"Repeated setCloseLock retains the original Timer reset");
        require(native.r100DoorMessageEnabled()==messageEnabled,"AEVB enable executes after exactly twenty resumed close sleeps");
        for(unsigned i=0;i<2;++i) {
          require(native.windows()[i].fenceFlags==windows[i].flags,"Both original SetEnableFence disable bits match");
          require(native.windowFenceEnabled(i,1)==!(windows[i].flags&0x40000000),"Player vault uses source fence bit");
          require(native.windowFenceEnabled(i,2)==!(windows[i].flags&0x20000000),"Enemy vault uses source fence bit");
        }
        for(const auto &event:native.drainEvents())if(event.kind==re4::InteriorEvent::ScriptDoorVoice) {
          ++totalNativeVoices;require(event.soundBlock==6 && event.soundNo==0x28 && event.slot==2,
              "Door-close warning uses the original room sound and authored door identity");
          for(unsigned k=0;k<3;++k)require(event.position[k]==door.pos[k],"Original door warning is positional");
        }
        require(totalNativeVoices==voiceCount,"Original door warning emits once in the exact source branch");
        if(native.doors()[0].scriptLocked)
          require(!native.doorOffer(player.feet(),player.heading()),"emDoorActEvtCk cannot offer a script-locked door");
        if(doorTask.done && windowTask.done && tick>=150)break;
      }
    }
    const auto &closure=areas.authoredArea(3),&knock=areas.authoredArea(0xb);
    re4::CollisionVec center{};
    for(const auto &point:closure.area.points) {center[0]+=point[0]*.25;center[2]+=point[1]*.25;}
    center[1]=closure.area.floor;
    require(areas.doorClosureFlagHit(center,0),"Actual AEV3 polygon latches Room_flg0 for the player/enemies");
    require(!areas.doorClosureFlagHit({center[0],center[1]+2000,center[2]},0),"Original flag area clips height");
    re4::RoomSaveState closed;closed.room.set(3);closed.room.set(10);
    bool foundKnock=false;
    for(int x=-1500;x<=1500;x+=100)for(int z=-1500;z<=1500;z+=100) {
      re4::CollisionVec feet{};
      for(const auto &point:knock.area.points) {feet[0]+=point[0]*.25;feet[2]+=point[1]*.25;}
      feet[0]+=x;feet[2]+=z;feet[1]=knock.area.floor;
      const auto enabled=areas.secondaryOffer(closed,feet,0,true,true,{},true);
      if(!enabled || enabled->area!=0xb)continue;
      foundKnock=true;require(enabled->kind==re4::R100SecondaryOffer::RoomMessage && enabled->message==0xb,
          "Enabled actual AEVB selects the original locked-door message");
      const auto disabled=areas.secondaryOffer(closed,feet,0,true,true,{},false);
      require(!disabled || disabled->area!=0xb,"Disabled AEVB cannot leak a door message");
      const auto held=areas.secondaryOffer(closed,feet,0,false,true,{},true);
      require(!held || held->area!=0xb,"Source door Action2 callback requires a fresh press");
    }
    require(foundKnock,"Actual AEVB has reachable original action positions");
    re4::RoomSaveState fresh;
    re4::RoomInteractions glass(0x100,room,placements,argv[2],fresh);
    probe.setObjectCollision(glass.collision());
    const auto &pane=glass.windows()[0];
    require(!glass.windowFenceEnabled(0,1) && !glass.windowFenceEnabled(0,2),
        "Initial original house window forbids player and enemy vaults");
    const auto hit=glass.target(world(pane.placement,0,700,-500),world(pane.placement,0,700,500));
    require(hit && hit->kind==re4::InteriorHit::Window && hit->slot==0,"Actual house glass resolves its authored hit rectangle");
    require(glass.damage(*hit,2,10) && pane.hp==500 && glass.damage(*hit,2,11) && pane.hp==0,
        "Fence script preserves source ET00 two-handgun-hit pane destruction");
    require(!glass.windowFenceEnabled(0,1),"Broken glass cannot bypass the live source vault disable bit");
    re4::SourceFlags<4> inside;inside.set(64);
    fresh.room.set(3);fresh.room.set(10);
    require(!glass.tickR100Scripts(fresh,inside,{}),"Inside house enables broken window without prematurely releasing ambush");
    require(glass.windowFenceEnabled(0,1) && glass.windowFenceEnabled(0,2),"Original post-truck flags enable both real traversal kinds");
    bool foundVault=false;
    for(float side:{-1.f,1.f})for(float distance:{500.f,650.f,800.f,950.f}) {
      const auto start=world(pane.placement,0,-1000,side*distance);
      const auto floor=collision.ground(start,2000,2000);
      if(!floor || !probe.place(floor->point))continue;
      player.setPlacement(probe.position(),pane.placement.rotation[1]+(side>0?PI:0));
      const auto offer=glass.windowOffer(player,probe);
      if(offer && offer->slot==0 && offer->brokenFence)foundVault=true;
    }
    require(foundVault,"Enabled broken original r100 pane offers its real short-fence traversal over actual SAT");
    std::cout<<"M37 r100 door/window: "<<checks<<" checks PASS; literal source fibers,14 source timelines and actual ETS/AEV/SAT.\n";
    return 0;
  } catch(const std::exception &error) {std::cerr<<"FAIL: "<<error.what()<<'\n';return 1;}
}
