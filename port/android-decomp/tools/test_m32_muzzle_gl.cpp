// Reuse only the real EGL context, state sentinels and pixel readers from the
// independently tested world sprite backend. Production muzzle draw is new.
#define main original_world_effects_gl_check
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreturn-type"
#include "test_world_effects_gl.cpp"
#pragma GCC diagnostic pop
#undef main
#include "weapon_preview.h"
#include "muzzle_effects.h"
int main(int argc,char **argv){try {
  if(argc!=5)throw std::runtime_error("Usage: m32_muzzle_gl_check WEP01 WEP02 WEP07 CORE");
  Egl egl;
  const auto core=RoomArchive::LoadDrs(argv[4]);
  const auto *e=core.find("EFF");require(e,"Original core effect source exists");
  const auto coreEff=core.copy(e-core.entries().data());
  constexpr Matrix34 parent{1,0,0,0,0,1,0,0,0,0,1,0};
  for(unsigned profile=0;profile<3;++profile) {
    WeaponPreview weapon(argv[profile+1],static_cast<WeaponKind>(profile));
    const auto source=BuildWeaponWorldEffects(weapon.effects(),coreEff);
    MuzzleEffects simulation(source);require(simulation.trigger(1),"Actual original weapon flash triggered");
    require(simulation.quads(WorldEffectCamera{}).empty(),"Queued shot emits no world-origin flash before the solved weapon matrix");
    const auto pending=simulation.state();
    require(pending.instances.size()==1 && pending.instances[0].pending && !pending.instances[0].age,
            "Shot remains explicitly pending before the first fixed gameplay tick");
    MuzzleEffects queuedRestore(source);
    require(queuedRestore.restore(pending),"Pause before first firing tick preserves the pending shot");
    auto invalidPending=pending;invalidPending.instances[0].state.frames=1;
    require(!queuedRestore.restore(invalidPending),"Pending shot rejects invented simulated state atomically");
    invalidPending=pending;invalidPending.instances[0].state.grids.push_back({});
    require(!queuedRestore.restore(invalidPending),"Pending weapon snapshot rejects unrelated water-grid state atomically");
    simulation.tick(parent);
    queuedRestore.tick(parent);
    WorldEffectCamera camera;camera.eye={0,0,2000};camera.forward={0,0,-1};
    const auto quads=simulation.quads(camera);require(!quads.empty(),"Original EST0 produces real flash quads");
    require(queuedRestore.state().instances[0].state.sprites.size()==simulation.state().instances[0].state.sprites.size(),
            "Restored pending shot emits the complete authored flash once");
    const std::array<float,3> center{-90,0,0};
    sentinel();CreateMuzzleEffectsGl(source);checkState();
    clear();const auto base=pixels();sentinel();
    DrawMuzzleEffectsGl(identity,center,.002f,quads);checkState();
    const auto flash=pixels();require(difference(base,flash)>30,"Actual WEP textures produce visible RGB flash/smoke");
    for(unsigned i=3;i<flash.size();i+=4)require(flash[i]==base[i],"Muzzle sprite preserves destination alpha");
    preview(profile==0?"m32-wep01":profile==1?"m32-wep02":"m32-wep07",flash);
    // The original old tests used identity placement, concealing immediate
    // EST records born at world(0,0,0). Exercise a village-sized placement
    // with both CPU spawn coordinates and the actual GLES flash pixels.
    constexpr std::array<float,3> offset{30000,8000,-21000};
    auto translatedParent=parent;
    for(unsigned k=0;k<3;++k)translatedParent[k*4+3]=offset[k];
    MuzzleEffects translated(source);require(translated.trigger(1),"Translated real shot triggered");
    translated.tick(translatedParent);
    const auto localState=simulation.state().instances[0].state;
    const auto translatedState=translated.state().instances[0].state;
    require(translatedState.sprites.size()==localState.sprites.size(),"Translated weapon emits the same original EST records");
    for(unsigned i=0;i<localState.sprites.size();++i)for(unsigned k=0;k<3;++k)
      require(std::abs(translatedState.sprites[i].position[k]-localState.sprites[i].position[k]-offset[k])<.02f,
              "Every immediate flash/smoke sprite starts at the real weapon placement, never world origin");
    auto movedCamera=camera;auto movedCenter=center;
    for(unsigned k=0;k<3;++k){movedCamera.eye[k]+=offset[k];movedCenter[k]+=offset[k];}
    clear();DrawMuzzleEffectsGl(identity,movedCenter,.002f,translated.quads(movedCamera));
    require(difference(pixels(),flash)<40,"Real GLES flash remains attached when weapon and view translate away from world origin");
    const auto movedSnapshot=translated.state();MuzzleEffects movedRestore(source);
    require(movedRestore.restore(movedSnapshot),"Nonidentity flash pose restores after graphics loss");
    clear();DrawMuzzleEffectsGl(identity,movedCenter,.002f,movedRestore.quads(movedCamera));
    require(difference(pixels(),flash)<40,"Restored nonidentity flash reproduces its original GLES pixels");
    constexpr Matrix34 turnedParent{0,0,1,30000,0,1,0,8000,-1,0,0,-21000};
    MuzzleEffects turned(source);require(turned.trigger(1),"Turned real shot triggered");turned.tick(turnedParent);
    const auto turnedState=turned.state().instances[0].state;
    require(turnedState.sprites.size()==localState.sprites.size(),"Yawed real weapon retains all source flash records");
    for(unsigned i=0;i<localState.sprites.size();++i) {
      const auto expected=TransformPoint(turnedParent,localState.sprites[i].position);
      for(unsigned k=0;k<3;++k)
        require(std::abs(turnedState.sprites[i].position[k]-expected[k])<.02f,
                "Immediate flash positions and their first displacement follow the real rotated weapon matrix");
      const auto &a=turnedState.sprites[i], &b=localState.sprites[i];
      require(std::abs(a.speed[0]-b.speed[2])<.0001f && std::abs(a.speed[1]-b.speed[1])<.0001f && std::abs(a.speed[2]+b.speed[0])<.0001f,
              "Smoke velocity rotates once with original weapon part3");
    }
    MuzzleEffects reference(source);require(reference.restore(simulation.state()),"Authored first-tick reference state restores");
    for(unsigned tick=0;tick<24;++tick) {
      reference.tick(parent);translated.tick(translatedParent);turned.tick(turnedParent);
      const auto referenceSnapshot=reference.state(), movedSnapshot=translated.state(), rotatedSnapshot=turned.state();
      require(referenceSnapshot.instances.size()==movedSnapshot.instances.size() && referenceSnapshot.instances.size()==rotatedSnapshot.instances.size(),
              "Placement never changes source muzzle-instance lifetime");
      if(referenceSnapshot.instances.empty())continue;
      const auto &referenceState=referenceSnapshot.instances[0].state;
      const auto &movedState=movedSnapshot.instances[0].state;
      const auto &rotatedState=rotatedSnapshot.instances[0].state;
      require(movedState.sprites.size()==referenceState.sprites.size() && rotatedState.sprites.size()==referenceState.sprites.size(),
              "Delayed source flash/smoke generators retain record counts after placement and rotation");
      for(unsigned i=0;i<referenceState.sprites.size();++i) {
        const auto expected=TransformPoint(turnedParent,referenceState.sprites[i].position);
        for(unsigned k=0;k<3;++k) {
          require(std::abs(movedState.sprites[i].position[k]-referenceState.sprites[i].position[k]-offset[k])<.03f,
                  "Released and delayed smoke remain at their real translated weapon coordinates");
          require(std::abs(rotatedState.sprites[i].position[k]-expected[k])<.03f,
                  "Released and delayed smoke retain the original yawed world physics");
        }
      }
    }
    clear();DrawMuzzleEffectsGl(identity,center,.002f,simulation.quads(camera));
    require(pixels()==flash,"Repeated render frame advances no muzzle simulation");
    egl.fresh();ForgetMuzzleEffectsGl();CreateMuzzleEffectsGl(source);
    clear();DrawMuzzleEffectsGl(identity,center,.002f,simulation.quads(camera));
    require(pixels()==flash,"Fresh EGL context preserves same actual firing tick");
    DestroyMuzzleEffectsGl();
  }
  std::cout<<"M32 actual muzzle GLES3 PASS: "<<checks<<'\n';return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
