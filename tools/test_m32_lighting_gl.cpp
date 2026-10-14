// Production GLES passes with original CORE shadow image and PL00 skeleton.
#define main previous_follow_main
#include "test_follow_camera_gl.cpp"
#undef main
#include "archive.h"
#include "room_lighting.h"
#include "room_lighting_gl.h"
#include "room_filter_gl.h"
#include "foot_shadow_gl.h"
#include "muzzle_effects.h"
#include <filesystem>
#include <cstring>
namespace {
void word(std::vector<unsigned char> &b,unsigned p,unsigned v){for(unsigned k=0;k<4;++k)b.at(p+k)=v>>(24-8*k);}
void real(std::vector<unsigned char> &b,unsigned p,float f){unsigned v;std::memcpy(&v,&f,4);word(b,p,v);}
std::shared_ptr<re4::RoomLighting> lightFixture() {
  std::vector<unsigned char> b(8+260+300);b[1]=1;b[2]=0x2c;b[3]=1;word(b,4,8);word(b,12,1);
  const unsigned q=268;b[q]=7;b[q+1]=4;b[q+3]=0x11;b[q+20]=255;b[q+23]=128;b[q+29]=42;
  real(b,q+8,5000);real(b,q+24,1);real(b,q+56,.75f);real(b,q+68,3);
  return std::make_shared<re4::RoomLighting>(b);
}
std::vector<unsigned char> data(const re4::RoomArchive &a,const char *tag){const auto *e=a.find(tag);check(e,"Original lighting asset subfile exists");return a.copy(e-a.entries().data());}
void sourceFilters() {
  const auto clear=[](unsigned value){glBindFramebuffer(GL_FRAMEBUFFER,0);glViewport(0,0,width,height);glDisable(GL_SCISSOR_TEST);
    glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glClearColor(value/255.f,value/255.f,value/255.f,1);glClear(GL_COLOR_BUFFER_BIT);};
  const auto middle=[](){return pixels().at((height/2*width+width/2)*4);};
  re4::RoomLightCut cut;cut.contrast={1,128,16};clear(100);
  check(re4::ApplyRoomFilterGl(width,height,cut,0) && std::abs(int(middle())-142)<=1,"Original Filter00 contrast subtracts byte bias then adds with source power");
  clear(100);re4::ApplyRoomFilterGl(width,height,cut,0);check(std::abs(int(middle())-142)<=1,"Contrast refresh never feeds its own brightened image back into history");
  re4::DestroyRoomFilterGl();cut.contrast={};cut.blurRate=128;clear(200);re4::ApplyRoomFilterGl(width,height,cut,0);
  clear(20);re4::ApplyRoomFilterGl(width,height,cut,1);const auto blurred=middle();
  check(std::abs(int(blurred)-110)<=1,"Source blur blends previous scene before copying next history");
  clear(20);re4::ApplyRoomFilterGl(width,height,cut,1);check(middle()==blurred,"Repeated paused frame does not accumulate temporal blur");
  const auto history=re4::SaveRoomFilterGl();re4::DestroyRoomFilterGl();re4::RestoreRoomFilterGl(history);
  clear(20);re4::ApplyRoomFilterGl(width,height,cut,1);check(middle()==blurred,"Original history and paused pixels survive GPU recreation");
  clear(20);re4::ApplyRoomFilterGl(width,height,cut,2);check(middle()<blurred,"One new source tick advances the blur history");
  cut.blurRate=0;check(!re4::ApplyRoomFilterGl(width,height,cut,2),"Disabled source filter clears temporal history");
  cut.blurRate=128;clear(30);re4::ApplyRoomFilterGl(width,height,cut,3);check(middle()==30,"Reenabled filter has no stale previous room image");
  re4::DestroyRoomFilterGl();
  const auto previousRoom=re4::RoomAtmosphere().data;
  const auto firstRoom=lightFixture(),nextRoom=lightFixture();re4::RoomAtmosphere().data=firstRoom;
  clear(200);re4::ApplyRoomFilterGl(width,height,cut,0,firstRoom);
  clear(20);re4::ApplyRoomFilterGl(width,height,cut,1,firstRoom);const auto roomHistory=re4::SaveRoomFilterGl();
  auto eventCut=cut;eventCut.blurRate=64;
  clear(20);re4::ApplyRoomFilterGl(width,height,eventCut,1,firstRoom);
  check(std::abs(int(middle())-65)<=1,"An event lighting change within the room retains its source history");
  re4::DestroyRoomFilterGl();re4::RestoreRoomFilterGl(roomHistory);
  clear(20);re4::ApplyRoomFilterGl(width,height,cut,1,firstRoom);
  check(middle()==blurred,"A room-owned paused frame survives GPU recreation");
  clear(20);re4::ApplyRoomFilterGl(width,height,cut,1,nextRoom);
  check(middle()==20,"A room change clears the previous room image even at the same source tick");
  re4::RoomAtmosphere().data=nextRoom;re4::RestoreRoomFilterGl(roomHistory);
  clear(30);re4::ApplyRoomFilterGl(width,height,cut,2,nextRoom);
  check(middle()==30,"Restoring another room cannot inject its previous image into this room");
  re4::DestroyRoomFilterGl();re4::RoomAtmosphere().data=previousRoom;
}
void shaderLights() {
  auto scene=scenery(fixture(10000,0));auto lighting=lightFixture();scene.lighting=lighting;
  re4::CreateSceneGl(scene);re4::SceneFollowHeading(0);re4::SceneSeedFollowView(0,0);
  re4::SceneProbe probe;probe.showBody=false;
  re4::DrawSceneGl(width,height,0,0,0,false,&probe,nullptr,0);const auto enabled=pixels();
  lighting->cuts[0].lights[0].kind=0;
  re4::DrawSceneGl(width,height,0,0,0,false,&probe,nullptr,0);
  check(pixels()==enabled,"Nonzero source Kind defaults enabled, identical to Kind0");
  lighting->cuts[0].lights[0].flags=0;
  re4::DrawSceneGl(width,height,0,0,0,false,&probe,nullptr,0);const auto dark=pixels();
  check(difference(enabled,dark)>1000,"Original Type4 raw coefficients contribute in actual scene GLES shader");
  lighting->cuts[0].lights[0].flags=7;lighting->cuts[0].lights[0].customAttenuation[3]=6;
  re4::DrawSceneGl(width,height,0,0,0,false,&probe,nullptr,0);const auto attenuated=pixels();
  check(difference(enabled,attenuated)>1000,"Custom GX K0 denominator reaches real output pixels");
  unsigned lower=0;for(std::size_t i=0;i<enabled.size();i+=4)lower+=attenuated[i]<enabled[i];
  check(lower>1000,"Doubling source K0 decreases light brightness");
  re4::DestroySceneGl();
  lighting->cuts[0].lights[0].attribute=4;
  for(auto &draw:scene.draws)draw.scrollLightAttribute=true;
  re4::CreateSceneGl(scene);
  re4::DrawSceneGl(width,height,0,0,0,false,&probe,nullptr,0);
  check(pixels()==dark,"SMX scroll attribute excludes original lamp attribute4 from actual scenery pixels");
  re4::DestroySceneGl();
  lighting->cuts[0].screenAmbient={64,48,32,255};
  for(auto &draw:scene.draws){draw.materialRGB={255,255,255};draw.noDiffuseLight=true;draw.scrollLightAttribute=false;}
  re4::CreateSceneGl(scene);
  re4::DrawSceneGl(width,height,0,0,0,false,&probe,nullptr,0);const auto ambient=pixels();
  unsigned ambientPixels=0;
  for(std::size_t p=0;p<ambient.size();p+=4)
    ambientPixels+=ambient[p]==63&&ambient[p+1]==47&&ambient[p+2]==31;
  check(ambientPixels>1000,"SMX flag0x10 retains original screen ambient instead of making unlit surfaces white");
  re4::DestroySceneGl();
  for(auto &draw:scene.draws)draw.vertexColors=true;
  for(std::size_t p=0;p<scene.colors.size();p+=4){scene.colors[p]=90;scene.colors[p+1]=70;scene.colors[p+2]=50;}
  re4::CreateSceneGl(scene);
  re4::DrawSceneGl(width,height,0,0,0,false,&probe,nullptr,0);const auto vertexAmbient=pixels();
  unsigned vertexPixels=0;
  for(std::size_t p=0;p<vertexAmbient.size();p+=4)
    vertexPixels+=vertexAmbient[p]==89&&vertexAmbient[p+1]==69&&vertexAmbient[p+2]==49;
  check(vertexPixels>1000,"SMX flag0x10 retains authored vertex ambient in the actual scenery shader");
  re4::DestroySceneGl();
  re4::Texture reflection;reflection.width=reflection.height=1;reflection.rgba={128,64,32,128};scene.textures.push_back(reflection);
  for(auto &draw:scene.draws){draw.vertexColors=false;draw.specularEnabled=true;draw.specularTexture=0;draw.specularRGB={255,255,255};}
  re4::CreateSceneGl(scene);re4::DrawSceneGl(width,height,0,0,0,false,&probe,nullptr,0);const auto coloredReflection=pixels();
  unsigned rgbPixels=0;for(std::size_t p=0;p<coloredReflection.size();p+=4)
    rgbPixels+=std::abs(int(coloredReflection[p])-95)<=1&&std::abs(int(coloredReflection[p+1])-59)<=1&&std::abs(int(coloredReflection[p+2])-35)<=1;
  check(rgbPixels>1000,"Original Specular Type0 adds sampled RGB times raster/material colour before final TEV/fog");
  re4::DestroySceneGl();for(auto &draw:scene.draws)draw.specularType=1;
  re4::CreateSceneGl(scene);re4::DrawSceneGl(width,height,0,0,0,false,&probe,nullptr,0);const auto alphaReflection=pixels();
  unsigned alphaPixels=0;for(std::size_t p=0;p<alphaReflection.size();p+=4)
    alphaPixels+=std::abs(int(alphaReflection[p])-95)<=1&&std::abs(int(alphaReflection[p+1])-71)<=1&&std::abs(int(alphaReflection[p+2])-47)<=1;
  check(alphaPixels>1000,"Original Specular Type1 adds sampled alpha times previous TEV colour");
  re4::DestroySceneGl();
}
void globalIlluminationShader() {
  re4::StaticScene scene;scene.vertices={-.8f,-.8f,0,.8f,-.8f,0,0,.8f,0};
  scene.normals={0,0,1,0,0,1,0,0,1};scene.uv={0,0,1,0,.5f,1};scene.colors.assign(12,255);
  re4::Texture base;base.width=base.height=1;base.rgba={255,255,255,255};scene.textures.push_back(base);
  base.rgba={128,64,32,255};scene.textures.push_back(base);
  re4::SceneDraw material;material.count=3;material.texture=0;scene.draws.push_back(material);
  std::vector<re4::SkinCorner> skin(3);
  for(auto &corner:skin)for(auto *weight:{&corner.position,&corner.normal}){weight->count=1;weight->rates[0]=1;}
  const std::vector<re4::Matrix34> pose{{1,0,0,0,0,1,0,0,0,0,1,0}};
  const std::vector<std::array<float,3>> bind{{0,0,0}};
  constexpr float camera[]{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};int owner{};
  auto lighting=lightFixture();lighting->cuts[0].actorAmbient={64,48,32,255};
  re4::RoomAtmosphere().data=lighting;re4::CreateGenericActorGl(&owner,scene,skin);
  re4::GenericActorStyle style;style.selectMask=0;style.globalIlluminationImage=1;
  const auto render=[&] {
    glViewport(0,0,width,height);glDepthMask(GL_TRUE);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
    glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    re4::DrawGenericActorGl(&owner,camera,{},1,pose,bind,pose[0],false,0x40,nullptr,1000,&style);return pixels();
  };
  const auto with=render();unsigned count=0;for(std::size_t p=0;p<with.size();p+=4)
    count+=std::abs(int(with[p])-63)<=1&&std::abs(int(with[p+1])-24)<=1&&std::abs(int(with[p+2])-8)<=1;
  check(count>1000,"Original global illumination multiplies previous TEV RGB by CORE RGB and scale2");
  style.globalIlluminationImage=-1;const auto without=render();check(difference(with,without)>1000,"Only actors with original GI flag use the extra stage");
  style.globalIlluminationImage=1;check(render()==with,"GI enable/disable restores source pixels without state leak");
  re4::DestroyGenericActorGl(&owner);
  for(unsigned vertex=0;vertex<3;++vertex){scene.normals[vertex*3]=.6f;scene.normals[vertex*3+1]=0;scene.normals[vertex*3+2]=.8f;}
  scene.textures[1].width=4;scene.textures[1].height=1;scene.textures[1].rgba={32,0,0,255,80,0,0,255,128,0,0,255,224,0,0,255};
  re4::CreateGenericActorGl(&owner,scene,skin);constexpr re4::Matrix34 scaled{2,0,0,0,0,2,0,0,0,0,2,0};
  glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
  re4::DrawGenericActorGl(&owner,camera,{},1,pose,bind,scaled,false,0x40,nullptr,1000,&style);
  const auto inverseNormal=pixels();unsigned inversePixels=0;
  // n.x=.6/2, texgen u=.65; linear texel2/3 interpolation gives137.6,
  // then GX raster63 * GI137.6/255 * original TEV scale2 rounds to68.
  for(std::size_t p=0;p<inverseNormal.size();p+=4)inversePixels+=std::abs(int(inverseNormal[p])-68)<=1 && inverseNormal[p+1]==0 && inverseNormal[p+2]==0;
  check(inversePixels>1000,"Scaled actor GI uses original inverse-transposed model normals instead of forward scaling their lookup");
  re4::DestroyGenericActorGl(&owner);re4::RoomAtmosphere().data.reset();
}
void materialAfterChannelClamp() {
  auto lighting=lightFixture();auto &cut=lighting->cuts[0];
  cut.actorAmbient=cut.screenAmbient={128,128,128,255};
  auto &lamp=cut.lights[0];lamp.flags=7;lamp.type=4;lamp.id=0;
  lamp.enableMask=0x50;lamp.radius=2000000;lamp.position={0,0,1000000};
  lamp.color={255,255,255,128};lamp.customAttenuation={1,0,0,1,0,0};
  re4::StaticScene scene;scene.vertices={-.8f,-.8f,0,.8f,-.8f,0,0,.8f,0};
  scene.normals={0,0,1,0,0,1,0,0,1};scene.uv={0,0,1,0,.5f,1};scene.colors.assign(12,255);
  re4::Texture white;white.width=white.height=1;white.rgba={255,255,255,255};scene.textures.push_back(white);
  re4::SceneDraw material;material.count=3;material.texture=0;scene.draws.push_back(material);
  std::vector<re4::SkinCorner> skin(3);
  for(auto &corner:skin)for(auto *weight:{&corner.position,&corner.normal}){weight->count=1;weight->rates[0]=1;}
  const std::vector<re4::Matrix34> pose{{1,0,0,0,0,1,0,0,0,0,1,0}};
  const std::vector<std::array<float,3>> bind{{0,0,0}};
  constexpr float camera[]{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};int owner{};
  re4::RoomAtmosphere().data=lighting;re4::CreateGenericActorGl(&owner,scene,skin);
  re4::GenericActorStyle style;style.color={.5f,.5f,.5f,1};style.selectMask=1;
  glViewport(0,0,width,height);glDepthMask(GL_TRUE);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
  glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
  re4::DrawGenericActorGl(&owner,camera,{},1,pose,bind,pose[0],false,0x40,nullptr,1000,&style);
  unsigned lit=0;const auto actor=pixels();for(std::size_t p=0;p<actor.size();p+=4)
    lit+=std::abs(int(actor[p])-128)<=1&&std::abs(int(actor[p+1])-128)<=1&&std::abs(int(actor[p+2])-128)<=1;
  check(lit>1000,"GX saturates ambient128 plus diffuse255 to255 before multiplying material128, giving128");
  re4::DestroyGenericActorGl(&owner);
  auto floor=scenery(fixture(10000,0));floor.lighting=lighting;lamp.position={0,1000000,0};
  for(auto &draw:floor.draws){draw.materialRGB={128,128,128};draw.vertexColors=false;draw.noDiffuseLight=false;}
  re4::CreateSceneGl(floor);re4::SceneFollowHeading(0);re4::SceneSeedFollowView(0,0);
  re4::SceneProbe probe;probe.showBody=false;re4::DrawSceneGl(width,height,0,0,0,false,&probe,nullptr,0);
  lit=0;const auto sceneryPixels=pixels();for(std::size_t p=0;p<sceneryPixels.size();p+=4)
    lit+=std::abs(int(sceneryPixels[p])-128)<=1&&std::abs(int(sceneryPixels[p+1])-128)<=1&&std::abs(int(sceneryPixels[p+2])-128)<=1;
  check(lit>1000,"Scenery saturates the channel before GX byte-valued material multiplication");
  re4::DestroySceneGl();re4::RoomAtmosphere().data.reset();
}
void shadows(const char *core,const char *pl,const char *wep,const char *room,const char *prefix) {
  const auto c=re4::RoomArchive::LoadDrs(core);const auto texture=std::make_shared<const re4::Texture>(re4::BuildFootShadowTexture(data(c,"EFF")));
  check(texture->width>0&&texture->rgba.size()==std::size_t(texture->width)*texture->height*4,"Actual CORE Esp0x13 decoded without generated texture");
  const auto roomArchive=re4::RoomArchive::Load(room);auto lighting=re4::ReadRoomLighting(roomArchive);
  auto floor=fixture(10000,0);re4::SatCollision collision(floor);re4::LeonPreview leon(pl,wep);leon.setPlacement({0,0,0},0);
  auto scene=scenery(floor);scene.lighting=lighting;scene.footShadowTexture=texture;
  re4::SceneProbe probe;probe.showBody=false;probe.collision=&collision;
  re4::CreateSceneGl(scene);re4::CreateActorGl(leon);re4::SceneFollowHeading(0);re4::SceneSeedFollowView(0,-1);
  re4::DrawSceneGl(width,height,0,0,0,false,&probe,&leon,0);const auto with=pixels();const auto state=re4::SaveSceneView();
  if(prefix)save(std::string(prefix)+"-shadow.ppm",with);
  check(state.footShadowColor==0,"Initially enabled source shadow owner is dark");
  const auto posed=re4::RoomAtmosphere().playerParts;
  const auto quads=re4::BuildPlayerFootShadows(lighting->cuts.at(0),posed,leon.feet(),0,0);
  check(!quads.empty()&&quads.size()==12,"Source six joint table and linked subdivisions make twelve original sprites");
  for(const auto &q:quads)check(q.size>=300&&q.size<=700&&q.position[1]==100&&q.alpha>=0&&q.alpha<=1,"r101 source shadow plane offset, table size and GX alpha retained");
  re4::DrawSceneGl(width,height,0,0,0,false,&probe,&leon,0);check(pixels()==with,"Frozen pose and source shadow draw are deterministic during pause");
  re4::DestroySceneGl();scene.footShadowTexture.reset();re4::CreateSceneGl(scene);re4::CreateActorGl(leon);re4::RestoreSceneView(state);
  re4::DrawSceneGl(width,height,0,0,0,false,&probe,&leon,0);const auto without=pixels();
  if(prefix)save(std::string(prefix)+"-no-shadow.ppm",without);
  std::cout<<"Foot-shadow pixel delta "<<difference(with,without)<<"; first plane "<<quads.front().position[0]<<','<<quads.front().position[1]<<','<<quads.front().position[2]<<"; alpha "<<quads.front().alpha<<'\n';
  check(difference(with,without)>50,"Original joint silhouettes visibly darken actual floor pixels");
  re4::DestroySceneGl();scene.footShadowTexture=texture;re4::CreateSceneGl(scene);re4::CreateActorGl(leon);re4::RestoreSceneView(state);
  re4::DrawSceneGl(width,height,0,0,0,false,&probe,&leon,0);check(pixels()==with,"Shadow texture and owner restore exactly after GPU recreation");
  GLboolean writes;glGetBooleanv(GL_DEPTH_WRITEMASK,&writes);
  check(writes&&glIsEnabled(GL_DEPTH_TEST)&&glGetError()==GL_NO_ERROR,"Shadow pass restores depth and texture state");
  re4::DestroySceneGl();
}
void muzzleLight(const char *core,const char *pl,const char *wep,const char *room) {
  const auto c=re4::RoomArchive::LoadDrs(core);
  const auto coreLighting=re4::ReadRoomLighting(c);
  check(coreLighting->present.at(8)&&coreLighting->cuts[8].lights.size()==4,
        "Actual CORE cut8 contains the four original effect lights");
  const auto &source=coreLighting->cuts[8].lights[0];
  check(source.type==2&&source.color==std::array<unsigned char,4>{255,255,255,128}&&source.intensity>70&&source.intensity<71,
        "Source muzzle light is CORE quadratic light0, not guessed warm point lighting");
  re4::WeaponPreview weapon(wep);weapon.sample(re4::WeaponMotion::Fire,0);
  const auto effects=re4::BuildWeaponWorldEffects(weapon.effects(),data(c,"EFF"));
  re4::MuzzleEffects muzzle(effects);re4::LeonPreview leon(pl,wep);leon.setPlacement({0,0,0},0);
  auto floor=fixture(4000,0);re4::SatCollision collision(floor);auto scene=scenery(floor);
  scene.lighting=re4::ReadRoomLighting(re4::RoomArchive::Load(room));
  re4::SceneProbe probe;probe.showBody=false;probe.collision=&collision;probe.coreLighting=coreLighting.get();probe.muzzle=&muzzle;
  re4::CreateSceneGl(scene);re4::CreateActorGl(leon);re4::SceneFollowHeading(0);re4::SceneSeedFollowView(0,-1);
  re4::DrawSceneGl(width,height,0,0,0,false,&probe,&leon,0);const auto baseline=pixels();
  check(muzzle.lights(*coreLighting).empty(),"Unfired weapon produces no scene light");
  check(muzzle.trigger(1),"Original weapon event accepted once");
  const auto attachment=re4::WeaponAttachmentMatrix(leon.rig().jointMatrices().at(10),leon.renderPlacement());
  const auto parent=re4::WeaponAttachmentMatrix(weapon.rig().jointMatrices().at(3),attachment);
  muzzle.tick(parent);const auto light=muzzle.lights(*coreLighting);
  check(light.size()==1&&light[0].radius==4000&&light[0].color==std::array<unsigned char,4>{255,205,165,64}&&
        light[0].type==source.type&&light[0].intensity==source.intensity&&light[0].flags==0x43,
        "Esp11 overrides source tint/radius/flags while retaining quadratic CORE intensity");
  const auto sourceFloat=[&](unsigned p) {
    const auto &b=effects.weaponLight->bytes;
    const auto bits=(unsigned(b[p])<<24)|(unsigned(b[p+1])<<16)|(unsigned(b[p+2])<<8)|b[p+3];
    float value;std::memcpy(&value,&bits,4);return value;
  };
  const auto origin=re4::TransformPoint(parent,{sourceFloat(12),sourceFloat(16),sourceFloat(20)});
  check(std::hypot(light[0].position[0]-origin[0],light[0].position[2]-origin[2])<1&&
        std::abs(light[0].position[1]-origin[1])<1,
        "Flash light follows real posed weapon part3 rather than the actor chest");
  re4::DrawSceneGl(width,height,0,0,0,false,&probe,&leon,0);const auto illuminated=pixels();
  check(difference(baseline,illuminated)>100,"Source Esp11 changes real scenery and actor GLES pixels without creating a billboard pass");
  std::cout<<"Original Esp11 light pixel delta "<<difference(baseline,illuminated)<<'\n';
  const auto state=muzzle.state();re4::MuzzleEffects restored(effects);check(restored.restore(state),"Exact muzzle fixed-tick state restores");
  probe.muzzle=&restored;re4::DrawSceneGl(width,height,0,0,0,false,&probe,&leon,0);
  check(pixels()==illuminated,"Paused/restored muzzlelight keeps the exact frame and weapon parent");
  const auto transientSlot=std::max(scene.lighting->maximumLights,unsigned(re4::CurrentRoomLightCut()->lights.size())+10);
  check(transientSlot<32,"Actual fixture flash pool slot is represented by its source scroll mask");
  // The real image test above proves the flash renders. Test its pool
  // mask directly on the actual linked lighting program: saturated room
  // surfaces cannot provide a reliable brighter-pixel oracle for masking.
  GLint previousProgram{};glGetIntegerv(GL_CURRENT_PROGRAM,&previousProgram);
  const auto lightingProgram=re4::RoomLightBindings().program;
  const auto countLocation=glGetUniformLocation(lightingProgram,"roomLightCount");
  check(countLocation>=0,"Actual linked source shader exposes its light count");
  glUseProgram(lightingProgram);re4::RoomLightBindings().valid=false;
  re4::BindRoomLights(lightingProgram,0x10,1u<<transientSlot,light[0].position,1);
  GLint selected{};glGetUniformiv(lightingProgram,countLocation,&selected);
  check(selected==1,"Original flash pool bit enables exactly its source GPU light");
  re4::RoomLightBindings().valid=false;
  re4::BindRoomLights(lightingProgram,0x10,0,light[0].position,1);
  glGetUniformiv(lightingProgram,countLocation,&selected);
  check(selected==0,"Masking the actual source flash pool bit disables it on the GPU");
  re4::RoomLightBindings().valid=false;glUseProgram(GLuint(previousProgram));
  std::cout<<"Source Esp11 pool mask PASS: slot "<<transientSlot<<"; current cut "<<re4::RoomAtmosphere().transition.cut<<'\n';
  restored.tick(parent);check(restored.lights(*coreLighting).empty(),"Handgun source Life_max1 destroys the effect-owned light at the next 30Hz tick");
  re4::DrawSceneGl(width,height,0,0,0,false,&probe,&leon,0);
  check(pixels()==baseline,"Expired light leaves no persistent brightening in scene or Leon");
  re4::DestroySceneGl();
}
void catalog(const char *directory) {
  unsigned rooms=0,foot=0,cast=0,bad=0;
  for(const auto &file:std::filesystem::directory_iterator(directory)) {
    const auto name=file.path().stem().string();if(file.path().extension()!=".dat"||name.size()!=4||name[0]!='r')continue;
    const auto lighting=re4::ReadRoomLighting(re4::RoomArchive::Load(file.path().string()));
    check(lighting->present[0],"Every real catalog cut0 stays valid after shadow/custom parsing");
    bool footRoom=false;for(const auto &l:lighting->cuts[0].lights)if(l.id==4){footRoom|=l.shadow.kind==5;cast+=l.shadow.kind!=5;}
    foot+=footRoom;bad+=lighting->unavailableCuts.size();++rooms;
  }
  check(rooms==127&&foot==104&&bad==26,"All127 LIT archives retain104 source foot-shadow rooms and26 unavailable r120 cuts");
  std::cout<<"Actual LIT catalog: "<<rooms<<"; foot-shadow rooms "<<foot<<"; unported cast-light records "<<cast<<"; unavailable later cuts "<<bad<<'\n';
}
}
int main(int argc,char **argv){try{
  if(argc!=5&&argc!=6&&argc!=7)throw std::runtime_error("Usage: m32_lighting_gl_check CORE PL00 WEP01 R101 [ALL_ROOMS [OUTPUT_PREFIX]]");
  Egl egl;sourceFilters();shaderLights();globalIlluminationShader();materialAfterChannelClamp();shadows(argv[1],argv[2],argv[3],argv[4],argc==7?argv[6]:nullptr);
  muzzleLight(argv[1],argv[2],argv[3],argv[4]);
  if(argc>=6)catalog(argv[5]);
  std::cout<<"M32 source lighting/foot-shadow GLES PASS: "<<checks<<'\n';
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
