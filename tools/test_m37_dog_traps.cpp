// Source boundary oracle and real Disc1 EM21/EM2A/Leon/SAT integration.
#include "archive.h"
#include "asset_reader.h"
#include "dog_traps.h"
#include "destruction_effects.h"
#include "turn_rules.h"
#include "weapon_preview.h"
#include <cstring>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <stdexcept>
namespace {
unsigned checks;
void require(bool x,const char *why){++checks;if(!x)throw std::runtime_error(why);}
std::vector<std::uint8_t> read(const char *path){std::ifstream f(path,std::ios::binary);if(!f)throw std::runtime_error("File missing");return {std::istreambuf_iterator<char>(f),{}};}
std::vector<std::uint8_t> data(const re4::RoomArchive &a,const char *tag){const auto *p=a.find(tag);require(p,"Actual source entry missing");return a.copy(p-a.entries().data());}
bool bearOracle(const re4::CollisionVec &a,const re4::CollisionVec &b,bool alive,bool damaged) {
  const float x=float(a[0])-float(b[0]),z=float(a[2])-float(b[2]);
  if(x*x+z*z>90000.f)return false;
  if(damaged || !alive)return false;
  if(std::fabs(float(a[1])-float(b[1]))>500.f)return false;
  return true;
}
bool wireOracle(const re4::CollisionVec &v,float len) {
  if(!(v[2]<0.f) && !(v[2]>len+100.f) && !(v[0]>400.f) && !(v[0]<-400.f) && !(v[1]>250.f) && !(v[1]<-2000.f))return true;
  return false;
}
}
int main(int argc,char **argv) {
  try {
    for(int x=-450;x<=450;x+=15)for(int z=-450;z<=450;z+=15)
      for(int y:{-501,-500,-1,0,1,500,501})for(unsigned flags=0;flags<4;++flags) {
        const re4::CollisionVec point{double(x),double(y),double(z)};
        require(re4::OriginalBearTrapContact({},point,flags&1,flags&2)==bearOracle({},point,flags&1,flags&2),"Literal em2aTrap1BiteCk boundary mismatch");
      }
    for(float len:{1.f,1000.f,1860.f,2160.f})for(int x:{-401,-400,-399,0,399,400,401})
      for(int y:{-2001,-2000,-1999,0,249,250,251})for(float z:{-1.f,0.f,1.f,len,len+99.f,len+100.f,len+101.f}) {
        const re4::CollisionVec v{x*1.,y*1.,z};
        require(re4::OriginalTripwireContact(v,len)==wireOracle(v,len),"Literal em2aTrap2HitCkPL inclusive box mismatch");
      }
    if(argc==1){std::cout<<"M37 trap source predicates: "<<checks<<" checks PASS\n";return 0;}
    if(argc!=7)throw std::runtime_error("Usage: m37_dog_traps_check r100.dat emleon00.esl EM21 EM2A PL00 WEP01");
    const auto room=re4::RoomArchive::Load(argv[1]);const re4::Sat sat(data(room,"SAT"));
    const re4::SatCollision collision(sat);const re4::EnemySpawns spawns(read(argv[2]));
    re4::ScenarioState scenario;std::set<unsigned> used;
    re4::DogTraps world(collision,spawns,argv[3],argv[4],scenario,&used);
    re4::LeonPreview player(argv[5],argv[6]);world.installPlayerMotions(player);
    re4::WalkProbe probe(collision,re4::WalkConfig{400,1600,2600,250,600,true});
    require(probe.reset(),"Actual SAT floor exists");
    require(world.actors().size()==8 && world.sources().size()==8,"All manually released original lists9..E/1D/7A exist");
    for(const auto &a:world.actors()) {
      require(!a->visible(),"Original trap list remains hidden before r100_trap_set");
      require(a->mesh().triangles>0 && !a->mesh().fallbackTriangles && !a->mesh().textures.empty(),"Actual textured BIN/TPL with no placeholder");
      require(a->rig() && !a->rig()->jointMatrices().empty(),"Actual modelInit rest skeleton owns native ARM64 pose");
      for(const auto &m:a->rig()->jointMatrices())for(float v:m)require(std::isfinite(v),"All actual rest matrices finite");
    }
    world.releaseAfterTruck();for(const auto &a:world.actors())require(a->visible(),"Original truck event releases dog and all7 traps");
    const auto armedEffects=world.drainEffects();
    require(armedEffects.size()==2 && armedEffects[0].sequence==5 && armedEffects[1].sequence==5,
        "Original type2 tripwire initialization emits persistent EST5 on both original wire actors");
    const auto trapArchive=re4::RoomArchive::LoadDrs(argv[4]);
    const auto coreArchive=re4::RoomArchive::LoadDrs((std::filesystem::path(argv[5]).parent_path().parent_path()/"etc/core.das").string());
    const auto fragments=re4::BuildTrapDestructionEffects(data(trapArchive,"EFF"),data(coreArchive,"EFF"));
    require(fragments.models.size()==2 && fragments.records.size()==13,"All actual FE Kaboom records and CORE models8/14");
    for(const auto &r:fragments.records)require(r.objectId==0x2a && (r.bytes[7]==0 || r.bytes[7]==2),"Original fragment bytes retain live parent parts0/2");
    re4::DestructionEffectsData merged;merged.room=0x100;re4::AppendDestructionData(merged,fragments);
    require(merged.models.size()==fragments.models.size() && merged.meshes.size()==fragments.meshes.size(),"Shared immutable destruction mesh indices rebase once");
    re4::DestructionEffectsSimulation destruction(merged,&collision,&collision);
    std::vector<re4::Matrix34> parts;
    for(const auto &matrix:world.actors()[1]->rig()->jointMatrices())parts.push_back(re4::WeaponAttachmentMatrix(matrix,world.actors()[1]->worldMatrix()));
    require(destruction.triggerTrap(4,0x200a,parts,41,0x1234) && !destruction.triggerTrap(4,0x200a,parts,41,0x1234),"Actual trap Kaboom accepted once with source posed posts");
    require(!destruction.instances().empty(),"Original model geometry fragments exist immediately on blast");
    for(unsigned i=0;i<120;++i) {
      for(const auto &instance:destruction.instances()) {
        require(instance.mesh<merged.meshes.size(),"Fragment mesh belongs to original owner");
        for(float value:instance.matrix)require(std::isfinite(value),"Original Kaboom floor/metal matrix remains finite");
      }
      destruction.tick();
    }
    re4::DestructionEffectsSimulation mixedDestruction(merged,&collision,&collision);
    const auto sourceFragment=std::find_if(merged.records.begin(),merged.records.end(),
        [](const auto &record){return record.sequence==4;});
    require(sourceFragment!=merged.records.end(),"Mixed EST has an actual source FE record");
    const auto parent=parts[sourceFragment->bytes[7]];
    require(mixedDestruction.triggerTrapRecord(4,sourceFragment->sourceRecord,0x200a,parent,42,1,0x1234),
        "Mixed sprite/model EST creates exactly one source-emitted FE with captured original seed");
    require(mixedDestruction.state().emitters.empty() && mixedDestruction.state().pending.empty(),
        "Direct FE callback does not duplicate the source mixed generator or delay schedule");
    require(!mixedDestruction.triggerTrapRecord(4,sourceFragment->sourceRecord,0x200a,parent,42,1,0x1234) &&
        mixedDestruction.triggerTrapRecord(4,sourceFragment->sourceRecord,0x200a,parent,42,2,0x1234),
        "Source burst emissions deduplicate by event and spawn, allowing the next original emission");
    require(mixedDestruction.restore(mixedDestruction.state()),"Mixed FE snapshot retains distinct emissions of one event");
    for(unsigned actor:{1u,2u}) {
      const auto &a=*world.actors()[actor];
      require(a.rig()->clipCount()==0,"Static tripwire does not fabricate animation curves");
      const auto model=re4::RoomArchive::LoadDrs(argv[4]);
      const auto body=re4::SkinnedModel(model.copy(4));
      const float scale=float(world.sources()[actor].hp)*.001f*.5f;
      for(unsigned part:{1u,2u}) {
        const float expected=body.mesh.joints[part].center[2]*scale;
        require(a.rig()->partPosition(part)[2]==expected,"Source hp length stretches only endpoint parts.pos.z");
        require(a.rig()->bindCenters()[part]==re4::RestJointCenters(body.mesh)[part],"Source wire length retains original inverse bind");
      }
    }
    const auto &dog=world.sources()[0];probe.setScriptPosition(dog.position);player.setPlacement(dog.position,float(dog.heading));
    require(world.rescueAvailable(player),"Original squared3,000-unit rescue offer");
    require(world.beginRescue(player,probe),"Original dog rescue begins");
    require(player.action()==re4::PlayerAction::DogRescue && !player.weaponVisible(),"Native kneeling FCV owns Leon and his empty hands");
    require(scenario.scenario.test(12) && world.takeRescueStreamRequest() && !world.takeRescueStreamRequest(),"SCF12 and exactly one actual block1/cueE jingle request");
    const auto expected=re4::TransformPoint(world.actors()[0]->worldMatrix(),{431.86002f,0,-989.88f});
    for(unsigned k=0;k<3;++k)require(float(player.feet()[k])==expected[k],"Exact plemTrapCancel source placement");
    const auto camera=player.actionCamera();require(bool(camera) && camera->fovDegrees==55,"Original rescue camera55 degrees");
    const auto eye=re4::TransformPoint(player.placement(),{2500,2000,0});
    for(unsigned k=0;k<3;++k)require(float(camera->eye[k])==eye[k],"Original plem21TrapCamMove source eye");
    const auto dogArchive=re4::RoomArchive::LoadDrs(argv[3]);
    re4::SkinnedModel sourceDog(dogArchive.copy(0));
    re4::AnimationRig release(sourceDog,std::vector<re4::AnimationBytes>{
        {dogArchive.copy(0x19-4),dogArchive.copy(0x1c-4)}});
    release.startActionFlags(0,3,0,5);
    re4::SmdPlacement releasePlacement;releasePlacement.scale={1,1,1};
    for(unsigned k=0;k<3;++k)releasePlacement.position[k]=float(dog.position[k]);
    releasePlacement.rotation[1]=float(dog.heading);
    unsigned releaseFrames=0;float releaseDistance=0;
    unsigned rescueTicks=0;re4::PlayerActionInput held;held.aimHeld=held.fireHeld=held.firePressed=true;
    for(unsigned i=0;i<250;++i) {
      const bool releasing=world.dogState()==re4::DogTraps::DogState::Releasing;
      if(releasing) {
        const auto delta=release.advanceRootAction();
        const auto matrix=re4::PlacementMatrix(releasePlacement);
        for(unsigned r=0;r<3;++r)for(unsigned c=0;c<3;++c)
          releasePlacement.position[r]+=matrix[r*4+c]*float(delta.translation[c]);
        releasePlacement.rotation[1]=re4::GameCubeLimitAngle(releasePlacement.rotation[1]+float(delta.yaw));
        ++releaseFrames;
      }
      world.tick(player,probe);player.step(probe,1,1,true,true,1./30,held);
      if(releasing) {
        const auto actual=world.actors()[0]->worldMatrix();
        for(unsigned k=0;k<3;++k)require(std::fabs(actual[k*4+3]-releasePlacement.position[k])<.01f,
            "Liberation applies every original EM21 release FCV root delta before running");
        releaseDistance=std::hypot(actual[3]-float(dog.position[0]),actual[11]-float(dog.position[2]));
      }
      if(player.dogTrapActive())++rescueTicks;
      if(i<180)require(player.shots()==0,"Held gun input cannot interrupt original rescue motion");
      for(const auto &a:world.actors())for(const auto &m:a->rig()->jointMatrices())for(float v:m)require(std::isfinite(v),"Actual rescue/flee matrices finite");
    }
    require(releaseFrames>180 && releaseDistance>100,"Actual release motion moves the dog out of the trap before its run");
    require(rescueTicks>170 && rescueTicks<200,"Original 185-frame Leon rescue, not a made-up timer");
    require(!player.dogTrapActive() && player.alive(),"Original rescue restores control");
    require(world.dogState()!=re4::DogTraps::DogState::Trapped && used.count(9),"Original release243-frame motion advances to fleeing and EmSetDie");
    for(unsigned i=0;i<1000 && world.dogState()!=re4::DogTraps::DogState::Gone;++i)world.tick(player,probe);
    require(world.dogState()==re4::DogTraps::DogState::Gone && !world.actors()[0]->visible(),"Actual flee-root/fence FCV completes and original alpha falls by0.1 to invisible");
    // Move Leon away before deliberately shooting a wire. Damage waits3
    // sourcecalls, emits2 radius3000 bombs, hides that instance and persists.
    probe.setScriptPosition({-80000,860,-39000});player.setPlacement(probe.position(),0);
    re4::DogTrapHit shot;shot.actor=1;
    require(world.applyShot(shot,41,2) && !world.applyShot(shot,41,2),"One source shot serial queued once");
    for(unsigned i=0;i<3;++i){world.tick(player,probe);require(world.drainBlasts().empty(),"Source 3-tick bomb wait remains silent");}
    world.tick(player,probe);const auto blasts=world.drainBlasts();
    require(blasts.size()==2 && blasts[0].radius==3000 && blasts[1].radius==3000,"Original two 3000-unit PlWepHitCheck2 blast spheres");
    require(blasts[0].position!=blasts[1].position && !world.actors()[1]->visible() && used.count(10),"Original blast centers/posts and EmSetDie persistence");
    require(world.drainBlasts().empty(),"Exploded wire emits no repeated damage");
    re4::DogTraps returned(collision,spawns,argv[3],argv[4],scenario,&used);returned.releaseAfterTruck();
    require(!returned.actors()[0]->visible() && !returned.actors()[1]->visible(),"Rescued dog and detonated wire remain absent after room re-entry");
    require(returned.actors()[2]->visible(),"Untouched original second wire remains armed after room re-entry");
    // Trigger a real bear trap at its original ESL position.
    const auto bear=returned.sources()[4].position;probe.setScriptPosition(bear);player.setPlacement(bear,0);
    for(unsigned i=0;i<31;++i)player.step(probe,0,0,false,false,1./30,{});
    const auto hp=player.life();returned.installPlayerMotions(player);returned.tick(player,probe);
    require(player.action()==re4::PlayerAction::BearTrapBite && player.life()<hp,"Source300 bear bite applies authored Leon/EM2A catch motion and HP loss");
    const auto biteCamera=player.actionCamera();require(bool(biteCamera) && player.actionCameraBlend()==.1f,
        "Original bear camera requests0.1 easing for its first120 source ticks");
    const auto &trapMatrix=returned.actors()[4]->worldMatrix();
    re4::SmdPlacement trapCameraPlacement;trapCameraPlacement.scale={1,1,1};
    trapCameraPlacement.position={trapMatrix[3],trapMatrix[7],trapMatrix[11]};trapCameraPlacement.rotation[1]=player.heading();
    const auto biteEye=re4::TransformPoint(re4::PlacementMatrix(trapCameraPlacement),{-1200,1300,-1000});
    for(unsigned k=0;k<3;++k)require(float(biteCamera->eye[k])==biteEye[k],"Original bear camera uses actual trap origin and player yaw");
    require(used.count(13),"Sprung bear trap source list persists");
    for(unsigned i=0;i<180;++i){returned.tick(player,probe);player.step(probe,0,0,false,false,1./30,{});}
    require(!player.dogTrapActive() && player.alive(),"Original bear bite finishes and restores player");
    std::cout<<"M37 original dog/traps: "<<checks<<" checks PASS\n";
  } catch(const std::exception &e) {std::cerr<<e.what()<<'\n';return 1;}
}
