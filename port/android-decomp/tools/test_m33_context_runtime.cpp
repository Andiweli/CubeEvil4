// Production controller, world-tick routing, GLES transforms and EGL recovery.
#define RE4_M27_ENTRY previous_m33_context_m27_main
#include "test_m27_runtime.cpp"
#undef RE4_M27_ENTRY
#include <filesystem>
namespace {
re4::StaticScene contextScene() {
  auto scene=re4::BuildStaticScene(*previewSmd,-1,previewCommon.get());
  re4::AttachSceneTextures(*previewSmd,scene,previewCommon.get());
  if(previewSmx)re4::ApplySceneSmx(*previewSmx,scene);
  if(previewEnvironment)re4::AttachSceneEnvironment(*previewEnvironment,scene);
  if(previewBlocks)re4::AppendStaticSceneLayer(scene,previewBlocks->mesh);
  if(previewObjects)re4::AppendRoomObjectScene(scene,*previewObjects);
  if(previewItems)re4::AppendRoomItemScene(scene,*previewItems);
  if(previewLooseItems)re4::AppendRoomItemScene(scene,*previewLooseItems);
  if(destructionData)re4::AppendDestructionScene(scene,*destructionData);
  return scene;
}
}
int main(int argc,char **argv) {
  using namespace m27_test;
  try {
    if(argc!=9)throw std::runtime_error("Usage: m33_context_runtime_check R101 R400 PL00 WEP01 roominfo CORE EM12 emleon00");
    Egl egl;Jni jni;jlong session=33331;
    Java_com_ast_re4decomp_NativeBridge_environmentData(&jni.env,nullptr,string(argv[6]));
    const auto discFiles=std::filesystem::path(argv[3]).parent_path().parent_path().string();
    Java_com_ast_re4decomp_NativeBridge_configureSubscreen(&jni.env,nullptr,string(discFiles.c_str()),1);
    Java_com_ast_re4decomp_NativeBridge_configureEnemy(&jni.env,nullptr,string(argv[7]),string(argv[8]));
    const auto create=[&](const char *path,const char *name,bool explore) {
      Java_com_ast_re4decomp_NativeBridge_configurePlayMode(&jni.env,nullptr,explore?JNI_TRUE:JNI_FALSE,session);
      Java_com_ast_re4decomp_NativeBridge_configureTestProfile(&jni.env,nullptr,explore?1:0,session);
      Java_com_ast_re4decomp_NativeBridge_createScene(&jni.env,nullptr,string(path),string(argv[3]),
          string(argv[4]),string(argv[5]),string(name),session);
      frozenPixels(jni);
      // The combat launcher asks Java's importer for the explicit test
      // placement. Complete that same asynchronous production handshake.
      if(!explore)finish(jni,name,path);
      require(javaException.empty()&&actorGpuReady&&roomLadders&&roomInteractions,
              "Production room loader installs complete native context owners");
      unpause(jni);controls(jni);
    };
    // A genuine r400 cold start has no prior ScenarioState or window hit.
    // R400Init::SetBreakModel sets ETC0 bit1 and removes its pane SAT.
    create(argv[2],"r400",true);
    {
      const auto pane=std::find_if(roomInteractions->windows().begin(),roomInteractions->windows().end(),
          [](const auto &w){return w.placement.slot==0;});
      require(pane!=roomInteractions->windows().end() && !pane->hp &&
              roomInteractions->state().object(0,1) && scenarioState.room(0x400).object(0,1),
              "Cold r400 applies original R400Init broken-window HP and ETC state");
      const auto from=point(pane->placement,0,700,-100),to=point(pane->placement,0,700,100);
      require(!roomInteractions->target(from,to,1),"Cold broken r400 pane does not accept another glass shot");
      require(!roomInteractions->collision()->line(from,to,re4::SatSubset::All,0,0),
              "Cold r400 window has no invisible dynamic SAT pane barrier");
      require(!roomInteractions->breakWindowsCrossed(from,to),"Cold r400 pane does not emit another break event");
      unsigned shards=0;for(const auto &event:roomInteractions->drainEvents())shards+=event.kind==re4::InteriorEvent::WindowBroken;
      require(!shards,"Original cold r400 broken model does not replay glass fragments");
    }
    ++session;create(argv[1],"r101",true);
    {
      const auto joints=leon->rig().jointMatrices();
      const auto feet=leon->feet();
      const auto curve=leon->rig().curveFrame();
      Java_com_ast_re4decomp_NativeBridge_setSubscreenOpen(&jni.env,nullptr,JNI_TRUE);
      require(subscreenPortraits.count(leon->equippedWeaponId())==1,
              "Paused GL owner prepares the source Leon menu portrait for cache-only UI access");
      const auto &portrait=subscreenPortraits.at(leon->equippedWeaponId());
      bool visible=false;
      for(std::size_t p=3;p<portrait.rgba.size();p+=4)visible=visible||portrait.rgba[p]!=0;
      require(visible,"Original menu portrait has visible original body and weapon pixels");
      ticks(jni,4);
      require(leon->feet()==feet && leon->rig().jointMatrices()==joints && leon->rig().curveFrame()==curve,
              "Portrait motion evaluation and open menu do not mutate the paused gameplay rig");
      Java_com_ast_re4decomp_NativeBridge_setSubscreenOpen(&jni.env,nullptr,JNI_FALSE);
      controls(jni);tick(jni);
      require(!subscreenOpen && !paused,"Closing the original menu resumes the existing world owner");
    }
    unsigned brokenBefore=0,entryBreaks=0;
    for(const auto &window:roomInteractions->windows())brokenBefore+=!window.hp;
    const auto placements=roomLadders->objectLadders();
    for(const auto &p:placements) {
      std::vector<unsigned> beforeHp;
      for(const auto &window:roomInteractions->windows())beforeHp.push_back(window.hp);
      bool found=false;
      for(float z:{600.f,800.f,1000.f}) {
        if(!walkProbe->place(point(p,0,0,z)))continue;
        leon->setPlacement(walkProbe->position(),p.rotation[1]+3.1415927f);
        const auto offer=roomLadders->offer(*leon,*walkProbe);
        if(offer&&offer->object&&offer->no==p.slot){found=true;break;}
      }
      require(found,"Production ladder approach resolves on the shared SAT layer");
      re4::SceneFollowHeading(leon->heading());re4::SceneSeedFollowView(0,0);
      controls(jni);frozenPixels(jni);controls(jni,re4::Bottom);tick(jni);controls(jni);
      require(leon->objectLadderActive(),"Physical A starts original object ladder climbing");
      unsigned t=0;while(leon->ladderActive()&&t++<400)tick(jni);
      require(!leon->ladderActive()&&!leon->ladderLandingFailed(),"Production climb returns to a real upper floor");
      for(unsigned index=0;index<beforeHp.size();++index)
        if(beforeHp[index]&&!roomInteractions->windows()[index].hp)++entryBreaks;
      leon->setPlacement(leon->feet(),p.rotation[1]);
      auto offer=roomLadders->offer(*leon,*walkProbe);
      // A normal dismount may finish farther back than obj13's top action
      // box. Approach the actual SAT landing within the original trigger.
      if(!offer||offer->no!=p.slot||offer->action!=re4::LadderAction::Tip) {
        const unsigned n=p.id==8?8:6;
        const auto top=point(p,0,n*533.3329-(p.id==8?1000:0),n*-194.1173);
        for(float z:{-700.f,-900.f,0.f,300.f}) {
          auto q=top;const auto offset=point(p,0,0,z);
          q[0]+=offset[0]-p.position[0];q[2]+=offset[2]-p.position[2];
          if(!walkProbe->place(q,500))continue;
          leon->setPlacement(walkProbe->position(),p.rotation[1]);
          offer=roomLadders->offer(*leon,*walkProbe);
          if(offer&&offer->no==p.slot&&offer->action==re4::LadderAction::Tip)break;
        }
      }
      require(offer&&offer->no==p.slot&&offer->action==re4::LadderAction::Tip,
              "Roof context selects source ladder tipping instead of generic jumping");
      controls(jni);frozenPixels(jni);controls(jni,re4::Bottom);tick(jni);controls(jni);
      if(!leon->ladderPropActive()) {
        const auto window=roomInteractions->windowOffer(*leon,*walkProbe);
        std::cerr<<"Ladder tip diagnostic slot "<<p.slot<<" action "<<int(leon->action())
                 <<" feet "<<leon->feet()[0]<<","<<leon->feet()[1]<<","<<leon->feet()[2]
                 <<" window "<<bool(window)<<'\n';
      }
      require(leon->ladderPropActive(),"Physical A runs the source ladder kick motion");
      ticks(jni,12);pause(jni);
      const auto motion=leon->ladderPropTicks();const auto delta=roomLadders->delta(p.slot);
      const auto before=frozenPixels(jni);auto *owner=roomLadders.get();
      require(delta.has_value(),"Tipping owns an animated world-space ladder draw transform");
      bool rendered=false;
      const auto scene=contextScene();
      std::array<float,3> center{};float extent=0;
      for(unsigned k=0;k<3;++k) {
        center[k]=scene.minimum[k]*.5f+scene.maximum[k]*.5f;
        extent=std::max(extent,scene.maximum[k]-scene.minimum[k]);
      }
      const auto scale=2/extent;
      calls.clear();frozenPixels(jni);
      for(const auto &draw:scene.draws)if(draw.roomObjectSlot==int(p.slot))
        for(const auto &call:calls)
          if(call.first<=GLint(draw.first)&&call.first+call.count>=GLint(draw.first+draw.count)) {
            for(unsigned r=0;r<3;++r)for(unsigned c=0;c<3;++c)
              require(std::abs(call.model[c*4+r]-(*delta)[r*4+c])<.0001,
                      "Production room shader receives the authored prop animation delta");
            for(unsigned r=0;r<3;++r) {
              double shift=(*delta)[r*4+3]-center[r];
              for(unsigned c=0;c<3;++c)shift+=double((*delta)[r*4+c])*center[c];
              require(std::abs(call.model[12+r]-float(shift*scale))<.0001,
                      "Production prop translation uses the scene VBO normalization");
            }
            rendered=true;
          }
      require(rendered,"Falling ladder remains submitted to GLES");
      egl.fresh();
      Java_com_ast_re4decomp_NativeBridge_createScene(&jni.env,nullptr,string(argv[1]),string(argv[3]),
          string(argv[4]),string(argv[5]),string("r101"),session);
      require(paused&&roomLadders.get()==owner&&leon->ladderPropTicks()==motion&&
                roomLadders->delta(p.slot)==delta&&pixelChanges(before,frozenPixels(jni))==0,
              "EGL recreation preserves the paused kick, prop motion and rendered pixels");
      unpause(jni);controls(jni);t=0;
      while(leon->ladderPropActive()&&t++<200)tick(jni);
      ticks(jni,80);
      require(!leon->ladderPropActive()&&roomLadders->objectBodies().size()+1==placements.size(),
              "Production source world tick disables the fallen ladder body");
      bool reset=false;
      for(float radius:{300.f,600.f,900.f,1200.f,1450.f})for(unsigned angle=0;angle<24&&!reset;++angle) {
        const auto a=angle*6.283185307179586/24;
        if(!walkProbe->place(point(p,float(std::cos(a)*radius),0,float(std::sin(a)*radius)),500))continue;
        for(float yaw:{-1.5707963f,1.5707963f,0.f}) {
          leon->setPlacement(walkProbe->position(),p.rotation[1]+yaw);
          const auto stand=roomLadders->offer(*leon,*walkProbe);
          if(stand&&stand->no==p.slot&&stand->action==re4::LadderAction::Reset){reset=true;break;}
        }
      }
      require(reset,"Production context offers standing up at a reachable real floor");
      controls(jni);frozenPixels(jni);controls(jni,re4::Bottom);tick(jni);controls(jni);
      require(leon->ladderPropActive(),"Physical A starts the authored ladder lift");
      t=0;while(leon->ladderPropActive()&&t++<200)tick(jni);ticks(jni,80);
      require(!roomLadders->delta(p.slot)&&roomLadders->objectBodies().size()==placements.size(),
              "Production lift restores original standing draw and collision");
    }
    unsigned brokenAfter=0;
    for(const auto &window:roomInteractions->windows())brokenAfter+=!window.hp;
    require(entryBreaks>0,"Production ladder entry shatters the crossed intact pane before later jumping out");
    require(brokenAfter>=brokenBefore+entryBreaks,"Later ladder actions preserve broken pane state");
    ++session;create(argv[2],"r400",false);
    require(encounter&&roomInteractions->room()==0x400,
            "Ganado combat mode retains its room window action owner");
    bool window=false;
    for(const auto &w:roomInteractions->windows())for(float z:{-650.f,650.f,-900.f,900.f}) {
      if(window||!walkProbe->place(point(w.placement,0,-1000,z),500))continue;
      leon->setPlacement(walkProbe->position(),w.placement.rotation[1]+(z>0?3.1415927f:0));
      const auto offer=roomInteractions->windowOffer(*leon,*walkProbe);if(!offer)continue;
      controls(jni);frozenPixels(jni);controls(jni,re4::Bottom);tick(jni);controls(jni);
      window=leon->windowActive();
    }
    require(window,"Physical A starts an original window vault while the Ganado test is active");
    // The original switches intact panes to windowOn and broken same-height
    // panes to fanceOn. Exercise the actual physical-A path in both rooms,
    // repeatedly, including the upstairs incoming path rejected by M33.
    for(unsigned room=0;room<2;++room) {
      ++session;create(argv[1+room],room?"r400":"r101",true);
      // Original LadderNearCk reserves a standing ladder's nearby window.
      // Tip the authored nearby props through physical A before traversing.
      for(const auto &p:roomLadders->objectLadders()) {
        const unsigned n=p.id==8?8:6;
        const auto top=point(p,0,n*533.3329f-(p.id==8?1000.f:0),n*-194.1173f);
        bool nearby=false;
        for(const auto &w:roomInteractions->windows())if(w.placement.slot==8||w.placement.slot==10) {
          double distance=0;for(unsigned k=0;k<3;++k)distance+=(top[k]-w.placement.position[k])*(top[k]-w.placement.position[k]);
          nearby|=distance<4000000.;
        }
        if(!nearby)continue;
        bool offered=false;
        for(float z:{-700.f,-900.f,0.f,300.f}) {
          auto q=top;const auto offset=point(p,0,0,z);
          q[0]+=offset[0]-p.position[0];q[2]+=offset[2]-p.position[2];
          if(!walkProbe->place(q,500))continue;
          leon->setPlacement(walkProbe->position(),p.rotation[1]);
          const auto offer=roomLadders->offer(*leon,*walkProbe);
          if(offer&&offer->no==p.slot&&offer->action==re4::LadderAction::Tip){offered=true;break;}
        }
        require(offered,"Nearby original standing ladder offers the source top kick");
        controls(jni);frozenPixels(jni);controls(jni,re4::Bottom);tick(jni);controls(jni);
        require(leon->ladderPropActive(),"Physical A tips the nearby ladder before window traversal");
        unsigned t=0;while(leon->ladderPropActive()&&t++<180)tick(jni);
        require(!leon->ladderPropActive()&&roomLadders->permitsWindowCross(top),
                "Tipped source ladder releases its window field reservation");
      }
      for(unsigned slot:{8u,10u}) {
        const auto found=std::find_if(roomInteractions->windows().begin(),roomInteractions->windows().end(),
            [&](const auto &w){return w.placement.slot==slot;});
        require(found!=roomInteractions->windows().end(),"Production round-trip window exists");
        const auto approach=[&](bool front) {
          std::optional<re4::InteriorWindowOffer> result;
          for(float z:{300.f,500.f,700.f,900.f})for(float x:{-450.f,0.f,450.f}) {
            if(result||!walkProbe->place(point(found->placement,x,-1000,front?z:-z)))continue;
            leon->setPlacement(walkProbe->position(),found->placement.rotation[1]+(front?3.1415927f:0));
            const auto offer=roomInteractions->windowOffer(*leon,*walkProbe);
            if(offer&&offer->slot==slot)result=offer;
          }
          return result;
        };
        require(approach(false).has_value(),"Intact outgoing source approach is on genuine SAT");
        controls(jni);frozenPixels(jni);controls(jni,re4::Bottom);tick(jni);controls(jni);
        require(leon->windowActive(),"First physical A uses original intact glass-vault FCV");
        unsigned t=0;while(leon->windowActive()&&t++<180)tick(jni);
        require(!leon->windowActive()&&!found->hp,"First actual vault commits the broken ETC pane");
        for(unsigned round=0;round<3;++round)for(bool front:{true,false}) {
          const auto offer=approach(front);
          require(offer&&offer->brokenFence,"Every return approach resolves the original broken-window fence action");
          const auto serial=windowEffectSerial;
          controls(jni);frozenPixels(jni);controls(jni,re4::Bottom);tick(jni);controls(jni);
          require(leon->fenceActive()&&!leon->windowActive(),
                  "Physical A selects short original fence motion for already broken panes");
          t=0;while(leon->fenceActive()&&t++<180)tick(jni);
          require(!leon->fenceActive()&&t<180&&leon->feet()==offer->landing,
                  "Repeated incoming/outgoing production action returns to its checked real floor");
          require(windowEffectSerial==serial&&!found->hp,
                  "Repeated passage does not create duplicate glass bursts or restore the pane");
        }
      }
    }
    require(leon->life()==1200,"Context and combat tests preserve unlimited HP");
    std::cout<<"M33 production context JNI/GLES PASS: "<<checks<<" checks\n";
  }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
}
