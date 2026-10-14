// Actual r101 shotgun-house side wall, original FCVs and production GLES skinning.
#include "actor_gl.h"
#include "room_pickups.h"
#include "walk_probe.h"
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
using V = re4::CollisionVec;
unsigned checks{}, frames{}, captures{};
double maximumGpuDifference{}, originalPenetration{}, correctedClearance{};
void require(bool ok, const char *why) {
  if (!ok) throw std::runtime_error(why);
  ++checks;
}
double dot(const V &a, const V &b) {
  return (a[0]*b[0]+a[1]*b[1])+a[2]*b[2];
}
V subtract(V a, const V &b) {
  for(unsigned k=0;k<3;++k) a[k]-=b[k];
  return a;
}
void equal(const V &a, const V &b, double tolerance, const char *why) {
  for(unsigned k=0;k<3;++k) require(std::abs(a[k]-b[k])<tolerance,why);
}
struct Egl {
  EGLDisplay display=EGL_NO_DISPLAY;
  EGLContext context=EGL_NO_CONTEXT;
  EGLSurface surface=EGL_NO_SURFACE;
  Egl() {
    setenv("EGL_PLATFORM","surfaceless",1);
    display=eglGetDisplay(EGL_DEFAULT_DISPLAY);
    EGLint major{},minor{},count{};
    require(eglInitialize(display,&major,&minor),"Wall EGL initialization");
    require(eglBindAPI(EGL_OPENGL_ES_API),"Wall EGL ES API");
    const EGLint attributes[]{EGL_SURFACE_TYPE,EGL_PBUFFER_BIT,
      EGL_RENDERABLE_TYPE,EGL_OPENGL_ES3_BIT,EGL_RED_SIZE,8,EGL_GREEN_SIZE,8,
      EGL_BLUE_SIZE,8,EGL_DEPTH_SIZE,24,EGL_NONE};
    EGLConfig config{};
    require(eglChooseConfig(display,attributes,&config,1,&count)&&count,
      "Wall EGL configuration");
    const EGLint ca[]{EGL_CONTEXT_CLIENT_VERSION,3,EGL_NONE};
    const EGLint sa[]{EGL_WIDTH,32,EGL_HEIGHT,32,EGL_NONE};
    context=eglCreateContext(display,config,EGL_NO_CONTEXT,ca);
    surface=eglCreatePbufferSurface(display,config,sa);
    require(context!=EGL_NO_CONTEXT&&surface!=EGL_NO_SURFACE&&
      eglMakeCurrent(display,surface,surface,context),"Wall EGL context");
  }
  ~Egl() {
    re4::DestroyActorGl();
    eglMakeCurrent(display,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);
    eglDestroySurface(display,surface); eglDestroyContext(display,context);
    eglTerminate(display);
  }
};
V skinPoint(const re4::LeonPreview &p,std::size_t corner) {
  const auto &raw=p.renderJointMatrices();
  const auto &bind=p.rig().bindCenters();
  const auto &weights=p.renderSkin().at(corner).position;
  V result{};
  for(unsigned influence=0;influence<3;++influence) {
    const auto joint=weights.joints[influence];
    for(unsigned row=0;row<3;++row) {
      double point=raw.at(joint)[row*4+3];
      for(unsigned col=0;col<3;++col)
        point+=double(raw[joint][row*4+col])*
          (double(p.renderMesh().vertices.at(corner*3+col))-bind.at(joint)[col]);
      result[row]+=double(weights.rates[influence])*point;
    }
  }
  return result;
}
V worldPoint(const re4::LeonPreview &p,const V &local) {
  const auto m=p.renderPlacement(); V out{};
  for(unsigned row=0;row<3;++row) {
    out[row]=m[row*4+3];
    for(unsigned col=0;col<3;++col) out[row]+=double(m[row*4+col])*local[col];
  }
  return out;
}
double meshPlaneMinimum(const re4::LeonPreview &p,const V &normal,const V &wall) {
  double result=std::numeric_limits<double>::infinity();
  for(const auto &draw:p.renderMesh().draws) if(p.visiblePart(draw.work))
    for(std::size_t c=draw.first;c<draw.first+draw.count;++c)
      result=std::min(result,dot(subtract(worldPoint(p,skinPoint(p,c)),wall),normal));
  return result;
}
void gpu(const re4::LeonPreview &p) {
  const float camera[]{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
  re4::DrawActorGl(camera,{0,0,0},.0001f,p,false);
  GLint depth{}; glGetIntegerv(GL_DEPTH_FUNC,&depth);
  require(glIsEnabled(GL_DEPTH_TEST)&&depth==GL_LEQUAL,
    "Production actor retains ordinary scene depth test");
  const auto corners=p.renderSkin().size(), bytes=corners*9*sizeof(float);
  GLuint buffer{}; glGenBuffers(1,&buffer);
  glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER,buffer);
  glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER,bytes,nullptr,GL_STREAM_READ);
  glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER,0,buffer);
  glEnable(GL_RASTERIZER_DISCARD); glBeginTransformFeedback(GL_TRIANGLES);
  glDrawArrays(GL_TRIANGLES,0,GLsizei(corners)); glEndTransformFeedback();
  glDisable(GL_RASTERIZER_DISCARD);
  require(glGetError()==GL_NO_ERROR,"Production wall-pose transform feedback");
  const auto *observed=static_cast<const float *>(glMapBufferRange(
    GL_TRANSFORM_FEEDBACK_BUFFER,0,bytes,GL_MAP_READ_BIT));
  require(observed,"Wall GPU vertex capture");
  double maximum=0;
  for(std::size_t corner=0;corner<corners;++corner) {
    const auto local=skinPoint(p,corner), world=worldPoint(p,local);
    for(unsigned row=0;row<3;++row) {
      require(std::isfinite(observed[corner*9+6+row]),"Finite posed world mesh");
      maximum=std::max(maximum,std::abs(local[row]-observed[corner*9+row]));
      maximum=std::max(maximum,std::abs(world[row]-observed[corner*9+6+row]));
    }
  }
  require(glUnmapBuffer(GL_TRANSFORM_FEEDBACK_BUFFER)==GL_TRUE,"Wall capture intact");
  glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER,0,0); glDeleteBuffers(1,&buffer);
  require(maximum<.025,"Rendered original parts match corrected collision world placement");
  maximumGpuDifference=std::max(maximumGpuDifference,maximum); ++captures;
}
void samePose(const re4::LeonPreview &a,const re4::LeonPreview &b) {
  require(a.action()==b.action(),"Wall handling preserves original action dispatch");
  const auto &x=a.rig().jointMatrices(), &y=b.rig().jointMatrices();
  require(x.size()==y.size(),"Original pose skeleton unchanged");
  for(unsigned part=0;part<x.size();++part) for(unsigned component=0;component<12;++component)
    require(std::memcmp(&x[part][component],&y[part][component],sizeof(float))==0,
      "Wall correction leaves FCV parts bit-identical");
  require(a.rig().status().sequenceFrame==b.rig().status().sequenceFrame &&
    a.rig().status().curveFrame==b.rig().status().curveFrame,
    "Wall correction preserves original motion frame");
}
}
int main(int argc,char **argv) { try {
  if(argc!=7) throw std::runtime_error("Usage: m36_player_wall_check PL00 WEP01 WEP02 WEP07 EM12 R101");
  const auto archive=re4::RoomArchive::Load(argv[6]); const auto *resource=archive.find("SAT");
  require(resource,"Actual r101 SAT present");
  const re4::Sat sat(archive.copy(resource-archive.entries().data()));
  const re4::SatCollision world(sat); re4::ScenarioState scenario;
  const re4::RoomPickups pickups(0x101,archive,scenario);
  const auto shotgun=std::find_if(pickups.items().begin(),pickups.items().end(),
    [](const auto &item){return item.id==0x2c;});
  require(shotgun!=pickups.items().end(),"Screenshot's original shotgun pickup exists");
  require(sat.polygons.size()>583,"Screenshot house collision present");
  const auto &polygon=sat.polygons[583]; const auto &rawNormal=sat.normals[polygon.normal];
  const V normal{rawNormal[0],rawNormal[1],rawNormal[2]};
  const auto &vertex=sat.vertices[polygon.vertices[0]]; const V wall{vertex[0],vertex[1],vertex[2]};
  require(normal[0]>.999&&std::abs(normal[1])<.001&&
    std::abs(shotgun->position[2]+17573.3)<1,"Actual screenshot house west wall fixture");
  V centroid{}; for(auto index:polygon.vertices) for(unsigned k=0;k<3;++k) centroid[k]+=double(sat.vertices[index][k])/3;
  V feet{centroid[0]+normal[0]*310,3200,centroid[2]+normal[2]*310};
  const auto floor=world.ground(feet,100,100);
  require(floor&&std::abs(floor->point[1]-3200)<.01,"Shotgun-house real upper floor");
  feet[1]=floor->point[1];
  Egl egl;
  for(unsigned profile=0;profile<3;++profile) {
    re4::LeonPreview player(argv[1],argv[2],argv[5],true), reference(argv[1],argv[2],argv[5],true);
    if(profile==1) { player.installHandgun(argv[3]);reference.installHandgun(argv[3]);
      require(player.equipWeapon(0x23)&&reference.equipWeapon(0x23),"Screenshot WEP02 grip"); }
    if(profile==2) { player.installShotgun(argv[4]);reference.installShotgun(argv[4]);
      require(player.equipWeapon(0x2c)&&reference.equipWeapon(0x2c),"Original shotgun grip"); }
    require(player.setWeaponAmmo(player.equippedWeaponId(),player.magazineCapacity(),20)&&
      reference.setWeaponAmmo(reference.equippedWeaponId(),reference.magazineCapacity(),20),
      "Actual source ammunition permits one shot and a real reload");
    require(player.weaponPowerLevel(player.equippedWeaponId())==0,
      "Fresh campaign firearm starts at original power nibble zero");
    require(player.setWeaponPowerLevel(player.equippedWeaponId(),profile)&&
      !player.setWeaponPowerLevel(player.equippedWeaponId(),7)&&
      !player.setWeaponPowerLevel(0xffff,2),"Original supported tuning nibble bounds");
    re4::WalkProbe probe(world,{300,1600,2600,250,600,true}), unchanged(world);
    require(probe.place(feet,1)&&unchanged.place(feet,1),"Old300-radius position genuinely accepted");
    player.setPlacement(feet,0);reference.setPlacement(feet,0);
    player.setRenderInterpolation(true);
    const auto baseline=meshPlaneMinimum(player,normal,wall);
    require(baseline<-20,"Old screenshot stance clips original arm/body mesh through wall");
    originalPenetration=std::min(originalPenetration,baseline);
    re4::CreateActorGl(player); gpu(player);
    bool sawDraw=false,sawAim=false,sawFire=false,sawReload=false,sawKnife=false;
    for(unsigned frame=0;frame<360;++frame) {
      re4::PlayerActionInput input;
      if(frame>=25&&frame<210) input.aimHeld=true;
      if(frame==75) input.firePressed=true;
      if(frame==135) input.reloadPressed=true;
      if(frame>=240&&frame<310) input.knifeHeld=true;
      if(frame==270) input.firePressed=true;
      const double turn=frame>=330?1:0;
      player.step(probe,turn,0,false,false,1./30,input);
      reference.step(unchanged,turn,0,false,false,1./30,input);
      samePose(player,reference);
      const auto center=player.sceneryBodyCenter();
      require(dot(subtract(center,wall),normal)>399.98,
        "Original posed part0 sphere keeps source400 wall clearance on every stationary tick");
      require(std::abs(player.feet()[1]-3200)<.01,"Pose wall handling keeps authored upper floor");
      equal(player.feet(),probe.position(),.01,"Collision feet and rendered actor placement agree");
      sawDraw|=player.action()==re4::PlayerAction::Draw;
      sawAim|=player.action()==re4::PlayerAction::Aim;
      sawFire|=player.action()==re4::PlayerAction::Fire;
      sawReload|=player.action()==re4::PlayerAction::Reload;
      sawKnife|=player.action()==re4::PlayerAction::KnifeSlash;
      if(frame==0) {
        correctedClearance=meshPlaneMinimum(player,normal,wall);
        require(correctedClearance>20,"Source body correction clears screenshot's idle arm/body mesh");
        equal(player.renderFeet(),player.feet(),.01,
          "Native collision correction is not interpolated back through the wall");
        player.step(probe,0,0,false,false,1./60,input);
        require(meshPlaneMinimum(player,normal,wall)>20,
          "Display half tick keeps the corrected actual idle mesh clear");
        gpu(player);
        // Restore the native phase without advancing motion. This fixture
        // checks interpolation, then independently compares one full tick.
        player.suspendClock();
      }
      if(frame==0||frame==35||frame==75||frame==135||frame==270||frame==320||frame==350) gpu(player);
      ++frames;
    }
    if(!(sawDraw&&sawAim&&sawFire&&sawReload&&sawKnife))
      std::cerr<<"profile="<<profile<<" draw="<<sawDraw<<" aim="<<sawAim
        <<" fire="<<sawFire<<" reload="<<sawReload<<" knife="<<sawKnife<<"\n";
    require(sawDraw&&sawAim&&sawFire&&sawReload&&sawKnife,
      "Actual draw/aim/shot/reload/knife FCV states were exercised at this wall");
    require(player.shots()==1&&player.magazine()==player.magazineCapacity(),
      "Wall correction preserves one trigger and source reload result");
    require(player.shotRay().powerLevel==profile&&player.laserRay().powerLevel==profile,
      "Damage shot retains its owning weapon firepower nibble");
    reference.inheritInventory(player);
    require(reference.weaponPowerLevel(player.equippedWeaponId())==profile,
      "Room reconstruction retains per-weapon power levels");
    re4::DestroyActorGl();
  }
  // Two real upright contacts: order cannot choose a farther actor over the
  // first actor on a movement segment. The old one-actor API remains identical.
  {
    re4::WalkProbe a(world), b(world), single(world);
    const V start{1000,3200,-16500};
    require(a.place(start,1)&&b.place(start,1)&&single.place(start,1),"Actual house actor-contact floor");
    const re4::ActorBlocker near{{1700,3200,-16500},200,1600}, far{{2300,3200,-16500},200,1600};
    a.setActorBlockers({far,near}); b.setActorBlockers({near,far}); single.setActorBlocker(near);
    a.moveDelta({1800,0,0});b.moveDelta({1800,0,0});single.moveDelta({1800,0,0});
    equal(a.position(),b.position(),.001,"All-actor contact is independent of registry order");
    equal(a.position(),single.position(),.001,"Single actor compatibility uses same earliest contact");
    require(std::abs(a.position()[0]-1200)<1,"Nearest actor stops before either original body overlaps");
    a.setActorBlockers({});a.moveDelta({200,0,0});
    require(a.position()[0]>1399,"Removing all actor owners releases old blockers");
  }
  {
    re4::WalkProbe clear(world),blocked(world);
    const V start{1000,3200,-16500};
    require(clear.place(start,1)&&blocked.place(start,1),"Actual house wall-slide floor");
    const re4::ActorBlocker contact{{760,3200,-14900},200,1600};
    // This actor misses the initial diagonal segment. The west wall deflects
    // that segment into its circle, so each subsequent slide must test again.
    blocked.setActorBlockers({{{3000,3200,-14900},200,1600},contact});
    clear.moveDelta({-1000,0,1600});blocked.moveDelta({-1000,0,1600});
    const auto end=blocked.position();
    require(clear.position()[2]>-14901,"Actual SAT wall permits the tangent travel");
    require(end[2]<-14950&&std::hypot(end[0]-contact.feet[0],end[2]-contact.feet[2])>=499.98,
      "Wall deflection cannot slide through another upright actor");
  }
  std::cout<<"M36 player wall: "<<checks<<" checks, "<<frames<<" actual pose ticks, "<<captures
    <<" GLES captures, baseline="<<originalPenetration<<", corrected="<<correctedClearance
    <<", GPU max="<<maximumGpuDifference<<"\n";
  return 0;
} catch(const std::exception &e) { std::cerr<<e.what()<<"\n";return 1; } }
