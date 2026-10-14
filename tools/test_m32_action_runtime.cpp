// Original window event through production controller/JNI/GLES, including
// its authored camera and glass-fragment owner across a lost EGL context.
#define RE4_M27_ENTRY previous_m27_action_main
#include "test_m27_runtime.cpp"
#undef RE4_M27_ENTRY
#include <set>

namespace m32_action {
using namespace m27_test;
struct Fixture {
  const char *path{}, *name{};
  unsigned slot{}, direction{};
  bool front{};
};
double distance(const re4::CollisionVec &a,const re4::CollisionVec &b) {
  double result=0;for(unsigned k=0;k<3;++k)result+=(a[k]-b[k])*(a[k]-b[k]);
  return std::sqrt(result);
}
double dot(const re4::CollisionVec &a,const re4::CollisionVec &b) {
  double result=0;for(unsigned k=0;k<3;++k)result+=a[k]*b[k];return result;
}
void camera() {
  const auto authored=leon->windowCamera();
  require(authored.has_value(),"Active original window FCV has its own event camera");
  const auto actual=re4::SceneFollowPose();
  require(distance(actual.eye,authored->eye)<.001&&distance(actual.target,authored->target)<.001&&
          std::abs(actual.fovDegrees-authored->fovDegrees)<1e-6,
          "Production GLES camera uses original FCV eye, target and FOV without follow-wall correction");
  require(std::abs(dot(actual.look,actual.look)-1)<1e-6&&
          std::abs(dot(actual.right,actual.right)-1)<1e-6&&
          std::abs(dot(actual.up,actual.up)-1)<1e-6&&
          std::abs(dot(actual.look,actual.right))<1e-6&&
          std::abs(dot(actual.look,actual.up))<1e-6&&
          std::abs(dot(actual.right,actual.up))<1e-6,
          "Authored rolled view retains an orthonormal source camera basis");
  auto look=authored->target;
  for(unsigned k=0;k<3;++k)look[k]-=authored->eye[k];
  const auto length=std::sqrt(dot(look,look));for(auto &v:look)v/=length;
  re4::CollisionVec right{-look[2],0,look[0]};
  const auto horizontal=std::hypot(right[0],right[2]);
  require(horizontal>.001,"Real tested camera track has a determinate source horizontal basis");
  for(auto &v:right)v/=horizontal;
  const re4::CollisionVec up{right[1]*look[2]-right[2]*look[1],
      right[2]*look[0]-right[0]*look[2],right[0]*look[1]-right[1]*look[0]};
  const auto c=std::cos(authored->roll),s=std::sin(authored->roll);
  for(unsigned k=0;k<3;++k) {
    require(std::abs(actual.right[k]-(right[k]*c+up[k]*s))<1e-6&&
            std::abs(actual.up[k]-(up[k]*c-right[k]*s))<1e-6,
            "Production view applies original CameraSetOrientationRoll signs");
  }
}
bool sameFragments(const re4::DestructionEffectsState &a,const re4::DestructionEffectsState &b) {
  if(a.signature!=b.signature||a.frames!=b.frames||a.serial!=b.serial||a.random!=b.random||
     a.events.size()!=b.events.size()||a.emitters.size()!=b.emitters.size()||
     a.pending.size()!=b.pending.size()||a.models.size()!=b.models.size())return false;
  for(unsigned i=0;i<a.events.size();++i)
    if(a.events[i].slot!=b.events[i].slot||a.events[i].serial!=b.events[i].serial)return false;
  for(unsigned i=0;i<a.emitters.size();++i) {
    const auto &x=a.emitters[i],&y=b.emitters[i];
    if(x.record!=y.record||x.slot!=y.slot||x.age!=y.age||x.next!=y.next||x.seed!=y.seed||
       x.placement!=y.placement||x.interval!=y.interval)return false;
  }
  for(unsigned i=0;i<a.pending.size();++i) {
    const auto &x=a.pending[i],&y=b.pending[i];
    if(x.record!=y.record||x.slot!=y.slot||x.remaining!=y.remaining||x.seed!=y.seed||
       x.placement!=y.placement)return false;
  }
  for(unsigned i=0;i<a.models.size();++i) {
    const auto &x=a.models[i],&y=b.models[i];
    if(x.record!=y.record||x.slot!=y.slot||x.age!=y.age||x.serial!=y.serial||x.seed!=y.seed||
       x.position!=y.position||x.rotation!=y.rotation||x.angularSpeed!=y.angularSpeed||
       x.burstCenter!=y.burstCenter||x.color!=y.color||x.sizeX!=y.sizeX||x.sizeY!=y.sizeY||
       x.sizeMultiplier!=y.sizeMultiplier||x.sizeStep!=y.sizeStep||x.matrix!=y.matrix||
       x.parts.size()!=y.parts.size())return false;
    for(unsigned j=0;j<x.parts.size();++j) {
      const auto &p=x.parts[j],&q=y.parts[j];
      if(p.position!=q.position||p.world!=q.world||p.rotation!=q.rotation||p.speed!=q.speed||
         p.angularSpeed!=q.angularSpeed||p.flag!=q.flag)return false;
    }
  }
  return true;
}
std::vector<re4::SceneDraw> draws() {
  auto scene=re4::BuildStaticScene(*previewSmd,-1,previewCommon.get());
  re4::ApplyRoomScriptVisibility(activeRoomId,
      static_cast<const re4::ScenarioState &>(scenarioState).room(activeRoomId),scene);
  re4::AttachSceneTextures(*previewSmd,scene,previewCommon.get());
  if(previewSmx)re4::ApplySceneSmx(*previewSmx,scene);
  if(previewEnvironment)re4::AttachSceneEnvironment(*previewEnvironment,scene);
  if(previewBlocks) {
    re4::AppendStaticSceneLayer(scene,previewBlocks->mesh);
    re4::ApplyRoomScriptVisibility(activeRoomId,
        static_cast<const re4::ScenarioState &>(scenarioState).room(activeRoomId),scene);
  }
  if(previewObjects)re4::AppendRoomObjectScene(scene,*previewObjects);
  if(previewItems)re4::AppendRoomItemScene(scene,*previewItems);
  if(previewLooseItems)re4::AppendRoomItemScene(scene,*previewLooseItems);
  re4::AppendDestructionScene(scene,*destructionData);
  return scene.draws;
}
std::optional<re4::InteriorWindowOffer> approach(unsigned slot,bool front) {
  for(const auto &window:roomInteractions->windows())if(window.placement.slot==slot)
    for(float x:{-350.f,0.f,350.f})for(float z:{450.f,650.f,850.f}) {
      if(!walkProbe->place(point(window.placement,x,-1000,front?z:-z)))continue;
      leon->setPlacement(walkProbe->position(),window.placement.rotation[1]+(front?3.1415927f:0));
      const auto offer=roomInteractions->windowOffer(*leon,*walkProbe);
      if(offer&&offer->slot==slot) {
        re4::SceneFollowHeading(leon->heading());re4::SceneSeedFollowView(0,0);return offer;
      }
    }
  return {};
}
}
int main(int argc,char **argv) {
  using namespace m32_action;
  try {
    if(argc!=10)throw std::runtime_error("Usage: m32_action_runtime_check R100 R101 PL00 WEP01 roominfo CORE EM12 emleon00 OUTPUT_PREFIX");
    Egl egl;Jni jni;jlong session=3203;
    Java_com_ast_re4decomp_NativeBridge_environmentData(&jni.env,nullptr,string(argv[6]));
    Java_com_ast_re4decomp_NativeBridge_configureEnemy(&jni.env,nullptr,string(argv[7]),string(argv[8]));
    const auto create=[&](const char *path,const char *name,bool fresh=true) {
      if(fresh)++session;
      controls(jni);
      Java_com_ast_re4decomp_NativeBridge_configurePlayMode(&jni.env,nullptr,JNI_TRUE,session);
      Java_com_ast_re4decomp_NativeBridge_configureTestProfile(&jni.env,nullptr,1,session);
      Java_com_ast_re4decomp_NativeBridge_createScene(&jni.env,nullptr,string(path),string(argv[3]),
          string(argv[4]),string(argv[5]),string(name),session);
      frozenPixels(jni);
      require(leon&&roomInteractions&&walking&&actorGpuReady&&destruction&&!encounter,
              "Actual direct-room JNI owns source windows, fragments and an enemy-free player");
    };
    std::vector<Fixture> fixtures;
    std::set<unsigned> directions;
    for(unsigned room=0;room<2;++room) {
      const auto path=argv[1+room];const auto name=room?"r101":"r100";
      create(path,name);unpause(jni);
      const auto windows=roomInteractions->windows();bool firstRoomFixture=true;
      for(const auto &window:windows)for(bool front:{false,true}) {
        const auto offer=approach(window.placement.slot,front);
        if(!offer||(!firstRoomFixture&&directions.count(offer->direction)))continue;
        fixtures.push_back({path,name,offer->slot,offer->direction,front});
        firstRoomFixture=false;directions.insert(offer->direction);
      }
    }
    require(directions==std::set<unsigned>{0,1,2},"Both real rooms supply all three original window event directions");
    for(const auto &fixture:fixtures) {
      create(fixture.path,fixture.name);unpause(jni);
      const auto offer=approach(fixture.slot,fixture.front);
      require(offer&&offer->direction==fixture.direction,"Fresh production session retains the original SAT window approach");
      const auto window=roomInteractions->windows().at(offer->index);
      require(window.hp>0,"Authored fixture begins with an intact window");
      re4::SkinnedModel body(re4::RoomArchive::LoadDrs(argv[3]).copy(0));
      re4::AnimationRig reference(body,roomInteractions->windowMotions());
      reference.startActionFlags(fixture.direction,0,0,0x201);
      auto referenceFeet=offer->start;double referenceHeading=offer->heading;
      unsigned referenceTicks=0;
      const auto sourceRoot=[&] {
        while(referenceTicks<leon->windowTicks()) {
          const auto root=reference.advanceRootAction();
          const auto c=std::cos(referenceHeading),s=std::sin(referenceHeading);
          referenceFeet[0]+=c*root.translation[0]+s*root.translation[2];
          referenceFeet[1]+=root.translation[1];
          referenceFeet[2]+=-s*root.translation[0]+c*root.translation[2];
          referenceHeading+=root.yaw;++referenceTicks;
        }
        if(leon->windowActive())require(distance(leon->feet(),referenceFeet)<.02,
            "Production window placement preserves original root motion at the event counter");
      };
      controls(jni);frozenPixels(jni);controls(jni,re4::Bottom);tick(jni);controls(jni);
      require(leon->windowActive()&&leon->windowSlot()==fixture.slot,
              "Physical A/Kreuz starts the authored window event through production JNI");
      sourceRoot();camera();
      const unsigned breakTick=fixture.direction==0?15:fixture.direction==1?16:13;
      bool checkedContext=false;unsigned broken=0;
      while(leon->windowActive()&&leon->windowTicks()<180) {
        const auto before=leon->windowTicks();
        const auto events=destruction->state().events.size();
        calls.clear();tick(jni);sourceRoot();
        const auto &actualWindow=roomInteractions->windows().at(offer->index);
        if(before<breakTick) {
          require(actualWindow.hp>0&&destruction->state().events.size()==events,
                  "Glass and shard event remain intact until the literal source break counter");
        }else if(before==breakTick) {
          require(actualWindow.hp==0&&destruction->state().events.size()==events+1,
                  "Source pre-MotionMove break changes pane and creates one glass burst on the same tick");
          const auto &event=destruction->state().events.back();
          require(event.slot==fixture.slot&&event.serial==windowEffectSerial,
                  "Production fragment owner consumes the same unique window event serial");
          require(!destruction->instances().empty(),"Source break emits actual original EFM glass parts immediately");
          bool submittedShard=false;
          for(const auto &draw:draws())if(draw.destructionMesh>=0&&submitted(draw))submittedShard=true;
          require(submittedShard,"Real GLES submits the source fragment geometry on the break frame");
          ++broken;
        }
        if(!leon->windowActive())break;
        camera();
        if(!checkedContext&&before==breakTick+4) {
          pause(jni);
          auto *actor=leon.get();auto *interior=roomInteractions.get();auto *fragments=destruction.get();
          const auto feet=leon->feet();const auto bones=leon->rig().bones();
          const auto counter=leon->windowTicks();const auto serial=windowEffectSerial;
          const auto flags=roomInteractions->state();const auto state=destruction->state();
          const auto eye=leon->windowCamera();const auto image=frozenPixels(jni);
          ticks(jni,3);
          require(leon->feet()==feet&&leon->rig().bones()==bones&&leon->windowTicks()==counter&&
                  sameFragments(state,destruction->state())&&windowEffectSerial==serial,
                  "Pause freezes authored body/camera, shard physics, timers and event serial");
          egl.fresh();create(fixture.path,fixture.name,false);
          require(paused&&leon.get()==actor&&roomInteractions.get()==interior&&destruction.get()==fragments&&
                  leon->feet()==feet&&leon->rig().bones()==bones&&leon->windowTicks()==counter&&
                  roomInteractions->state().etc==flags.etc&&sameFragments(state,destruction->state())&&
                  windowEffectSerial==serial&&leon->windowCamera()->eye==eye->eye&&
                  leon->windowCamera()->target==eye->target,
                  "EGL recreation preserves original event camera and the complete native glass simulation");
          require(pixelChanges(image,frozenPixels(jni))==0,
                  "Mid-vault original camera and glass have exact framebuffer parity after context recreation");
          camera();save(std::string(argv[9])+"-window-direction-"+std::to_string(fixture.direction)+".ppm");
          unpause(jni);controls(jni);checkedContext=true;
        }
      }
      require(!leon->windowActive()&&!leon->windowCamera()&&broken==1&&checkedContext&&leon->life()==1200,
              "Resumed original vault terminates once and releases authored camera with unlimited health");
      require(distance(leon->feet(),offer->landing)<1&&walkProbe->position()==leon->feet(),
              "Original motion finishes at its real prevalidated SAT landing");
    }
    std::cout<<"M32 production window camera/glass JNI/GLES PASS: "<<checks<<" checks, "
             <<fixtures.size()<<" authored paths\n";return 0;
  }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
}
