// Original assets, actual production JNI input/room lifecycle and GLES3.
#define main previous_bridge_test_main
#include "test_android_bridge_gl.cpp"
#undef main

namespace m27_test {
using namespace bridge_test;
struct DrawCall {
  GLint first; GLsizei count; std::array<float,16> model;
  GLenum mode{};
  GLint lighting{}, fog{}, textured{}, alphaTest{}, imageUnit{}, alphaUnit{}, texture{}, alphaTexture{}, vao{};
  GLboolean depthWrite{};
};
std::vector<DrawCall> calls;
// Test-only omissions compare the real shadow against the same floor and
// lighting. Empty for every ordinary render and older regression.
std::vector<std::array<GLint, 2>> omittedDraws;
re4::CollisionVec point(const re4::RoomObjectPlacement &p, float x, float y, float z) {
  re4::SmdPlacement pose;
  pose.position=p.position; pose.rotation=p.rotation; pose.scale={1,1,1};
  const auto v=re4::TransformPoint(re4::PlacementMatrix(pose),{x,y,z});
  return {v[0],v[1],v[2]};
}
void unpause(Jni &jni) {
  if (paused) {
    controls(jni,re4::Start); frozenPixels(jni);
    controls(jni); frozenPixels(jni);
  }
  require(!paused,"Native Start resumes the room");
}
void finish(Jni &jni, const char *name, const char *path) {
  const auto request=token(jni,name);
  Java_com_ast_re4decomp_NativeBridge_finishRoomRequest(
      &jni.env,nullptr,jlong(request),string(path),string(""));
  frozenPixels(jni);
  require(roomRequest.token==0 && actorGpuReady,"Production room completion consumed safely");
}
void placeDoor(Jni &jni, unsigned slot, bool front=true) {
  unpause(jni);
  const auto &door=roomInteractions->doors().at(slot-1);
  const auto feet=point(door.placement,-650,0,front ? -650 : 650);
  require(walkProbe->place(feet),"Real door controller approach is SAT/capsule-valid");
  leon->setPlacement(walkProbe->position(),door.placement.rotation[1]+(front ? 0 : 3.1415927f));
  re4::SceneFollowHeading(leon->heading());
  re4::SceneSeedFollowView(0,0);
  controls(jni); ticks(jni,2);
}
std::vector<re4::SceneDraw> objectDraws() {
  auto scene=re4::BuildStaticScene(*previewSmd,-1,previewCommon.get());
  re4::AttachSceneTextures(*previewSmd,scene,previewCommon.get());
  if(previewSmx) re4::ApplySceneSmx(*previewSmx,scene);
  if(previewEnvironment) re4::AttachSceneEnvironment(*previewEnvironment,scene);
  re4::AppendRoomObjectScene(scene,*previewObjects);
  return scene.draws;
}
bool submitted(const re4::SceneDraw &draw) {
  for(const auto &call:calls)
    if(call.first <= GLint(draw.first) &&
       call.first+call.count >= GLint(draw.first+draw.count)) return true;
  return false;
}
void checkVariants(Jni &jni, unsigned slot, bool broken) {
  calls.clear(); frozenPixels(jni);
  unsigned checked=0;
  for(const auto &draw:objectDraws()) {
    if(draw.roomObjectSlot != int(slot) || draw.roomObjectVariant < 0) continue;
    require(submitted(draw)==(draw.roomObjectVariant==(broken ? 1:0)),
            "Actual GLES submits only the live original window variant");
    ++checked;
  }
  require(checked>=2,"Both original window states were checked against real draw commands");
}
void checkDoorModel(Jni &jni,unsigned slot) {
  calls.clear(); frozenPixels(jni);
  bool found=false;
  const auto yaw=roomInteractions->doors().at(slot-1).yaw;
  for(const auto &draw:objectDraws()) if(draw.roomObjectSlot==int(slot))
    for(const auto &call:calls)
      if(call.first<=GLint(draw.first) && call.first+call.count>=GLint(draw.first+draw.count)) {
        require(std::abs(call.model[0]-std::cos(yaw))<1e-5 &&
                  std::abs(call.model[8]-std::sin(yaw))<1e-5,
                "Actual room shader receives the door's live hinge rotation");
        found=true;
      }
  require(found,"Moving original door was submitted to GLES");
}
} // namespace m27_test
extern "C" void __real_glDrawArrays(GLenum,GLint,GLsizei);
extern "C" void __wrap_glDrawArrays(GLenum mode,GLint first,GLsizei count) {
  GLint program=0; glGetIntegerv(GL_CURRENT_PROGRAM,&program);
  const auto location=program ? glGetUniformLocation(GLuint(program),"modelDelta") : -1;
  if(location>=0) {
    m27_test::DrawCall call{first,count,{}};
    call.mode=mode;
    const auto get=[&](const char *name) { GLint v=0; const auto p=glGetUniformLocation(GLuint(program),name); if(p>=0) glGetUniformiv(GLuint(program),p,&v); return v; };
    call.lighting=get("roomLightingEnabled"); call.fog=get("roomFogType");
    call.textured=get("textured"); call.alphaTest=get("alphaTest");
    call.imageUnit=get("image"); call.alphaUnit=get("alphaImage");
    glGetBooleanv(GL_DEPTH_WRITEMASK,&call.depthWrite);
    GLint active=0; glGetIntegerv(GL_ACTIVE_TEXTURE,&active);
    glActiveTexture(GL_TEXTURE0); glGetIntegerv(GL_TEXTURE_BINDING_2D,&call.texture);
    glActiveTexture(GL_TEXTURE1); glGetIntegerv(GL_TEXTURE_BINDING_2D,&call.alphaTexture);
    glActiveTexture(GLenum(active)); glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&call.vao);
    glGetUniformfv(GLuint(program),location,call.model.data());
    m27_test::calls.push_back(call);
  }
  if (mode == GL_TRIANGLES)
    for (const auto &range : m27_test::omittedDraws)
      if (first == range[0] && count == range[1]) return;
  __real_glDrawArrays(mode,first,count);
}
#ifndef RE4_M27_ENTRY
#define RE4_M27_ENTRY main
#endif
int RE4_M27_ENTRY(int argc,char **argv) {
  using namespace m27_test;
  try {
    if(argc!=9) throw std::runtime_error("Usage: m27_runtime_check R101 PL00 WEP01 roominfo CORE EM12 emleon00 R301");
    Egl egl; Jni jni;
    jlong session=2701;
    Java_com_ast_re4decomp_NativeBridge_environmentData(&jni.env,nullptr,string(argv[5]));
    Java_com_ast_re4decomp_NativeBridge_configureEnemy(&jni.env,nullptr,string(argv[6]),string(argv[7]));
    const auto create=[&] {
      Java_com_ast_re4decomp_NativeBridge_createScene(&jni.env,nullptr,
          string(argv[1]),string(argv[2]),string(argv[3]),string(argv[4]),string("r101"),session);
      frozenPixels(jni);
      require(leon && roomInteractions && walking && actorGpuReady,
              "Direct catalog r101 starts the full native interior/player owner");
    };
    create();
    Java_com_ast_re4decomp_NativeBridge_audioActive(&jni.env,nullptr,JNI_TRUE);
    placeDoor(jni,1);
    const auto ammo=leon->magazine();
    controls(jni,re4::Bottom); tick(jni);
    require(leon->action()==re4::PlayerAction::DoorOpen,"Mapped A/Kreuz starts original hand opening");
    controls(jni); ticks(jni,20);
    checkDoorModel(jni,1);
    auto *const firstOwner=roomInteractions.get();
    auto *const firstPlayer=leon.get();
    pause(jni);
    const auto frozenPlayer=actionSnapshot();
    const auto frozenYaw=roomInteractions->doors()[0].yaw;
    const auto frozenPhase=roomInteractions->doors()[0].phase;
    const auto frozenFrame=roomInteractions->doors()[0].motion->sequenceFrame();
    const auto pixels=frozenPixels(jni);
    ticks(jni,3);
    require(actionSnapshot()==frozenPlayer && roomInteractions->doors()[0].yaw==frozenYaw &&
              roomInteractions->doors()[0].motion->sequenceFrame()==frozenFrame,
            "Pause freezes both original actor and door motions");
    egl.fresh(); create();
    require(paused && leon.get()==firstPlayer && roomInteractions.get()==firstOwner &&
              actionSnapshot()==frozenPlayer && roomInteractions->doors()[0].yaw==frozenYaw &&
              roomInteractions->doors()[0].phase==frozenPhase &&
              roomInteractions->doors()[0].motion->sequenceFrame()==frozenFrame,
            "Fresh real EGL context retains the exact paused door/player owners and frames");
    require(pixelChanges(pixels,frozenPixels(jni))==0,"Door's paused framebuffer is exact after context recreation");
    unpause(jni); ticks(jni,45);
    require(leon->action()==re4::PlayerAction::Footwork && leon->magazine()==ammo &&
              roomInteractions->doors()[0].phase==re4::InteriorDoorPhase::Open,
            "Resumed door motions finish once and preserve ammunition");
    Java_com_ast_re4decomp_NativeBridge_input(&jni.env,nullptr,0,0,1,0,0,0,0);
    ticks(jni,22);
    const auto &d=roomInteractions->doors()[0].placement;
    const auto start=point(d,-650,0,-440);
    const auto toward=point(d,-650,0,1000);
    double progress=0;
    for(unsigned k=0;k<3;++k) progress+=(leon->feet()[k]-start[k])*(toward[k]-start[k])/1440;
    require(progress>800,"Actual held left-stick walk crosses the opened original door");
    controls(jni); frozenPixels(jni);
    ++session; create();
    placeDoor(jni,2,false);
    controls(jni,re4::Bottom); tick(jni);
    controls(jni); ticks(jni,12);
    controls(jni,re4::Bottom); tick(jni);
    require(leon->action()==re4::PlayerAction::DoorKick,
            "Second A edge switches original hand motion to kick-open frame6");
    controls(jni); ticks(jni,45);
    require(leon->action()==re4::PlayerAction::Footwork &&
              roomInteractions->doors()[1].phase==re4::InteriorDoorPhase::Open &&
              roomInteractions->doors()[1].yaw < -1.5,
            "Original kick hit-frame opens the correct backside and releases the action");
    checkDoorModel(jni,2);
    const auto &lock=roomInteractions->doors().back();
    const re4::PlayerShot lockShot{900,point(lock.placement,-1150,975,-1200),
                                     point(lock.placement,-1150,975,1200)};
    routeRoomWeaponHits(&lockShot); afterWorldTick(&lockShot);
    require(!lock.lockHp && scenarioState.room(0x101).object(11,2) &&
              soundMixer.hasCue(6,0x52) && soundMixer.hasCue(6,0x53),
            "Production nearest-hit router commits original lock flag and installed lock sounds");
    const auto &window=roomInteractions->windows()[1];
    require(window.placement.slot==8,"Original reachable window profile selected");
    pause(jni); checkVariants(jni,8,false);
    const re4::PlayerShot glass1{901,point(window.placement,0,700,-100),
                                   point(window.placement,0,700,100)};
    routeRoomWeaponHits(&glass1); afterWorldTick(&glass1);
    require(window.hp==500 && !scenarioState.room(0x101).object(8,1),
            "Production handgun routing preserves first-hit original window hp");
    routeRoomWeaponHits(&glass1); afterWorldTick(nullptr);
    require(window.hp==500,"Repeated render callback cannot turn one shot into two window hits");
    auto glass2=glass1; glass2.serial=902;
    routeRoomWeaponHits(&glass2); afterWorldTick(&glass2);
    require(window.hp==0 && scenarioState.room(0x101).object(8,1) &&
              soundMixer.hasCue(6,0x2e) && soundMixer.hasCue(6,0x3b),
            "Second original window hit selects broken BIN and its actual crack/glass cues");
    checkVariants(jni,8,true);
    auto *const sourceOwner=roomInteractions.get();
    const auto sourceAction=actionSnapshot();
    const auto sourceFlags=roomInteractions->state().etc;
    const auto brokenPixels=frozenPixels(jni);
    egl.fresh(); create();
    require(roomInteractions.get()==sourceOwner && actionSnapshot()==sourceAction &&
              roomInteractions->state().etc==sourceFlags && paused &&
              pixelChanges(brokenPixels,frozenPixels(jni))==0,
            "Context restore neither replenishes windows/lock nor replays their old models");
    Java_com_ast_re4decomp_NativeBridge_requestDoorTest(&jni.env,nullptr);
    failNextUniformAllocation=true;
    finish(jni,"r301",argv[8]);
    require(activeRoomId==0x101 && roomInteractions.get()==sourceOwner &&
              roomInteractions->state().etc==sourceFlags && actionSnapshot()==sourceAction && paused,
            "Failed destination GPU restores source interaction collision/flags/player ownership");
    checkVariants(jni,8,true);
    Java_com_ast_re4decomp_NativeBridge_requestDoorTest(&jni.env,nullptr);
    finish(jni,"r301",argv[8]);
    require(activeRoomId==0x301 && !roomInteractions,
            "Successful room switch releases the previous borrowed dynamic walls");
    Java_com_ast_re4decomp_NativeBridge_requestVillageTest(&jni.env,nullptr);
    finish(jni,"r101",argv[1]);
    require(activeRoomId==0x101 && roomInteractions && !roomInteractions->windows()[1].hp &&
              !roomInteractions->doors().back().lockHp && roomInteractions->state().etc==sourceFlags,
            "Return to r101 loads the same persistent window/lock words after another room");
    checkVariants(jni,8,true);
    ++session; create();
    require(roomInteractions->windows()[1].hp==1000 && roomInteractions->doors().back().lockHp>=3,
            "Explicit fresh test session resets prototype progress exactly once");
    std::cout<<"M27 production JNI/GLES lifecycle PASS: "<<checks
             <<" checks; original doors through A/LS, mirrored kick, lock/window nearest hits, "
               "real variant/model uniforms, pause/EGL and GPU rollback/room return\n";
    re4::DestroySceneGl();
    return 0;
  } catch(const std::exception &e) {
    std::cerr<<"M27 production runtime FAIL: "<<e.what()<<'\n';
    return 1;
  }
}
