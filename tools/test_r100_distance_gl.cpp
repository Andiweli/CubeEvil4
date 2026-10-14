// Real GLES draw tests with the original r100/r120 LIT cuts. The fixtures
// isolate distance fog, alpha foliage, material saturation and native LOD.
#define main previous_follow_camera_gl_main
#include "test_follow_camera_gl.cpp"
#undef main
#include "event_playback.h"
#include "room_lighting_gl.h"
#include "world_effects_gl.h"
#include "room_area_effects.h"
#include "event_mirror_gl.h"
#include "cinematic_frame.h"
#include "room_objects.h"
#include "room_items.h"
#include <cstring>
#include <filesystem>

namespace {
using Color=std::array<unsigned char,4>;
void cinematicFrame() {
  re4::EventCamera camera;camera.eye={0,0,0};camera.target={0,0,-1};
  camera.fovRadians=1.5707963267948966f;camera.valid=true;
  auto source=re4::EventMirrorCameraMatrix(camera,{},1),cropped=source;
  re4::CropOriginalCinematicFrame(cropped.data());
  for(float row:{56.f,224.f,393.f}) {
    const float y=(1-2*row/448.f)*10000;
    const std::array<float,4> p{0,y,-10000,1};
    std::array<float,4> clip{};
    for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c)clip[r]+=cropped[c*4+r]*p[c];
    const float expectedY=1-2*(row-56)/337;
    check(std::abs(clip[1]/clip[3]-expectedY)<.000001f,
        "Original cinematic active rows56..393 fill the physical display exactly");
  }
  for(unsigned col=0;col<4;++col)for(unsigned row:{0u,2u,3u})
    check(source[col*4+row]==cropped[col*4+row],
        "Cinematic crop preserves horizontal lens, mirror source projection and GX fog depth");
}
re4::StaticScene plane(float depth) {
  re4::StaticScene scene;const float d=depth*.5f;
  scene.vertices={-d,-d,-depth,d,-d,-depth,d,d,-depth,
      -d,-d,-depth,d,d,-depth,-d,d,-depth};
  scene.minimum={-d,-d,-depth};scene.maximum={d,d,-depth};
  scene.colors.assign(24,255);
  for(unsigned i=0;i<6;++i)scene.normals.insert(scene.normals.end(),{0,0,1});
  scene.uv={0,0,1,0,1,1,0,0,1,1,0,1};
  re4::SceneDraw draw;draw.count=6;draw.blendMode=4;
  draw.noDiffuseLight=true;draw.materialRGB={40,80,120};
  scene.draws.push_back(draw);scene.triangles=2;return scene;
}
Color sample(re4::StaticScene &scene,int w=512,int h=448,std::uint32_t blocks=0xffffffffu,
             re4::RoomAreaEffects *areas=nullptr,const re4::WorldEffectsData *areaData=nullptr) {
  re4::CreateSceneGl(scene);
  if(areaData)re4::CreateRoomAreaEffectsGl(*areaData);
  re4::EventCamera camera;camera.eye={0,0,0};camera.target={0,0,-1};
  camera.fovRadians=1.5707963267948966f;camera.valid=true;
  re4::SceneProbe probe;probe.showBody=false;probe.roomCamera=&camera;
  probe.roomBlocks=blocks;
  probe.roomAreaEffects=areas;
  re4::DrawSceneGl(w,h,0,0,0,false,&probe,nullptr,0);
  Color c{};glReadPixels(w/2,h/2,1,1,GL_RGBA,GL_UNSIGNED_BYTE,c.data());
  check(glGetError()==GL_NO_ERROR,"Original distance GLES draw/read has no error");
  re4::DestroySceneGl();return c;
}
bool near(Color a,Color b,int tolerance=3) {
  for(unsigned i=0;i<3;++i)if(std::abs(int(a[i])-int(b[i]))>tolerance)return false;
  return true;
}
std::shared_ptr<re4::RoomLighting> isolated(const re4::RoomLightCut &source) {
  // Retain the real source fog/register values; isolate unrelated filter
  // and material stages so the expected result is independent of renderer.
  std::vector<unsigned char> bytes(268);bytes[1]=1;bytes[2]=0x2c;bytes[7]=8;
  auto result=std::make_shared<re4::RoomLighting>(bytes);
  result->cuts[0]=source;auto &cut=result->cuts[0];
  cut.screenAmbient=cut.actorAmbient={255,255,255,255};cut.lights.clear();
  cut.tevScale={0,0};cut.farRatio=0;cut.blurRate=0;
  cut.contrast={0,0,0};cut.focusLevel=0;return result;
}
Color expected(const re4::RoomFog &fog,float depth,Color before) {
  double x=std::clamp((double(depth)-fog.start)/(fog.end-fog.start),0.,1.);
  double a=x;
  switch(fog.type&7) {
    case 0:a=0;break;case 4:a=1-std::exp2(-8*x);break;
    case 5:a=1-std::exp2(-8*x*x);break;
    case 6:a=std::exp2(-8*(1-x));break;
    case 7:a=std::exp2(-8*(1-x)*(1-x));break;
    default:break;
  }
  const int amount=int(std::round(a*256));
  for(unsigned i=0;i<3;++i)before[i]=(before[i]*(256-amount)+fog.color[i]*amount)>>8;
  return before;
}
Color billboard(const re4::RoomLightCut &cut,float depth,unsigned type,bool room,
                unsigned alpha=255,bool screen=false) {
  re4::RoomAtmosphere()={};re4::RoomAtmosphere().data=isolated(cut);
  re4::WorldEffectsData data;re4::WaterAnimation animation;
  re4::Texture image;image.width=image.height=1;image.rgba={40,80,120,std::uint8_t(alpha)};
  animation.images={image};data.sprites.animations={animation};
  re4::WorldEffectQuad q;q.color={1,1,1,1};q.type=type;q.flags=0x300001;q.screen=screen;
  const float d=depth*.5f;
  q.position={{{-d,d,-depth},{d,d,-depth},{d,-d,-depth},{-d,-d,-depth}}};
  if(screen)q.position={{{0,0,0},{512,0,0},{512,448,0},{0,448,0}}};
  q.uv={{{0,0},{1,0},{1,1},{0,1}}};
  re4::EventCamera camera;camera.eye={0,0,0};camera.target={0,0,-1};
  camera.fovRadians=1.5707963267948966f;camera.valid=true;
  const auto matrix=re4::EventMirrorCameraMatrix(camera,{},1);
  glViewport(0,0,512,448);glDisable(GL_SCISSOR_TEST);glColorMask(1,1,1,1);glDepthMask(1);
  glClearColor(cut.fog.color[0]/255.f,cut.fog.color[1]/255.f,cut.fog.color[2]/255.f,1);
  glClearDepthf(1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
  if(room) {
    re4::CreateRoomAreaEffectsGl(data);re4::RoomAreaEffectQuad wrapped;wrapped.quad=q;
    re4::DrawRoomAreaEffectsGl(matrix.data(),{},1,{wrapped});re4::DestroyRoomAreaEffectsGl();
  } else {
    re4::CreateEventEffectsGl(data);re4::DrawEventEffectsGl(matrix.data(),{},1,{q});re4::DestroyEventEffectsGl();
  }
  Color result{};glReadPixels(256,224,1,1,GL_RGBA,GL_UNSIGNED_BYTE,result.data());
  check(glGetError()==GL_NO_ERROR,"Original billboard fog GLES draw/read has no error");
  re4::RoomAtmosphere()={};return result;
}
void billboardDistances(const re4::RoomLightCut &cut) {
  for(float depth:{500.f,10000.f,40000.f,100000.f,200000.f}) {
    const auto reference=expected(cut.fog,depth,{40,80,120,255});
    for(unsigned type:{0u,8u,72u}) {
      check(near(billboard(cut,depth,type,false),reference),
          "Original event tree, tiled and falling-leaf sprites inherit the same GX distance fog");
      check(near(billboard(cut,depth,type,true),reference),
          "Original room SST foliage inherits the same GX distance fog");
    }
    check(near(billboard(cut,depth,0,false,0),cut.fog.color,0),
        "Sprite alpha compare precedes fog and preserves transparent foliage holes");
    check(near(billboard(cut,depth,15,false),{40,80,120,255},0),
        "Esp0f explicitly disables room fog for the original lens pass");
  }
  check(near(billboard(cut,100,0,false,255,true),expected(cut.fog,100,{40,80,120,255})),
      "Normal screen sprite inherits original GX fog registers with its original zero depth");
}
void distances(const re4::RoomLightCut &cut) {
  for(float depth:{500.f,10000.f,40000.f,100000.f,200000.f}) {
    auto scene=plane(depth);scene.lighting=isolated(cut);
    const auto opaque=sample(scene);
    const auto reference=expected(cut.fog,depth,{40,80,120,255});
    if(!near(opaque,reference))std::cerr<<"Fog type="<<cut.fog.type<<" start="<<cut.fog.start
        <<" end="<<cut.fog.end<<" depth="<<depth<<" actual="<<int(opaque[0])<<','<<int(opaque[1])<<','<<int(opaque[2])
        <<" reference="<<int(reference[0])<<','<<int(reference[1])<<','<<int(reference[2])<<'\n';
    check(near(opaque,reference),"Opaque distance follows independently evaluated original fog curve");
    re4::Texture white;white.width=white.height=1;white.rgba={255,255,255,255};
    scene.textures={white,white};scene.draws[0].texture=0;
    scene.draws[0].alphaTest=true;scene.draws[0].alphaTexture=1;scene.draws[0].alphaReference=128;
    check(sample(scene)==opaque,"Alpha-masked foliage receives exactly the same original distance fog");
    scene.textures[1].rgba[3]=0;
    Color background=cut.fog.type?cut.fog.color:Color{0,0,0,0};
    check(near(sample(scene),background,0),"Transparent foliage holes preserve the original fog background");
  }
}
Color over(Color front,Color back,unsigned alpha) {
  for(unsigned k=0;k<3;++k)back[k]=std::uint8_t(std::lround((front[k]*alpha+back[k]*(255-alpha))/255.));
  return back;
}
re4::Texture pixelTexture(Color color) {
  re4::Texture image;image.width=image.height=1;
  for(auto channel:color)image.rgba.push_back(channel);
  return image;
}
void itemAlphaEdges(const re4::RoomArchive &room,const re4::RoomLightCut &cut) {
  const auto *entry=room.find("ITM");check(entry!=nullptr,"Actual r100 item archive exists");
  re4::RoomItemArchive items(room.copy(entry-room.entries().data()));
  const auto alpha=re4::DecodeTpl(items.tpl(6),1);
  check(alpha.format==0,"Green herb uses the original I4 alpha mask");
  for(unsigned mask:{0u,17u,68u,136u,255u}) {
    check(std::any_of(alpha.rgba.begin()+3,alpha.rgba.end(),[&](unsigned char a){return a==mask;}),
        "Regression alpha value occurs in the supplied original herb mask");
    auto scene=plane(10000),leaf=plane(2000);scene.lighting=isolated(cut);
    scene.draws[0].object=1;
    auto white=pixelTexture({255,255,255,255});
    auto edge=white;edge.rgba[3]=std::uint8_t(mask);leaf.textures={white,edge};
    auto &draw=leaf.draws[0];draw.texture=0;draw.alphaTest=true;draw.alphaTexture=1;
    draw.alphaReference=0;draw.blendMode=0;draw.materialRGB={20,60,10};
    draw.authored=true;draw.orderType=0;draw.object=2;draw.sortCenter={0,0,-2000};
    re4::AppendStaticSceneLayer(scene,leaf);
    const auto reference=over(expected(cut.fog,2000,{20,60,10,255}),
        expected(cut.fog,10000,{40,80,120,255}),mask);
    check(near(sample(scene),reference,3),
        "Original herb edge alpha blends over the already rendered background, without a clear-color rim");
  }
}
void spriteGlassOrder(const re4::RoomLightCut &cut) {
  // A source-format static plain sprite isolates the common OT0x11 used
  // by the forest and the glass; front/behind cases prevent a blanket fix.
  for(float depth:{1000.f,10000.f})for(unsigned alpha:{0u,85u,255u}) {
    re4::RoomAreaEffectsData data;data.banks.push_back({0,65535,0});
    data.effects.sequences={0};data.maskAnimations={{std::nullopt}};
    auto image=pixelTexture({60,150,30,255});
    re4::WaterAnimation animation;animation.images={image};animation.pattern={0};animation.duration={255};animation.loop=1;
    data.effects.sprites.animations={animation};re4::WaterRecord record;record.bytes[0]=1;record.bytes[7]=0xfe;
    record.bytes[11]=1;record.bytes[178]=record.bytes[179]=255;
    for(unsigned k=0;k<4;++k)record.bytes[156+k]=255;
    const auto real=[&](unsigned p,float f){std::uint32_t bits;std::memcpy(&bits,&f,4);
      for(unsigned k=0;k<4;++k)record.bytes[p+k]=std::uint8_t(bits>>(24-k*8));};
    real(20,-depth);real(48,1);real(136,depth);real(140,depth);real(152,1);
    for(unsigned k=0;k<4;++k)real(160+k*4,1);
    re4::WaterTarget target;target.records={record};data.effects.sprites.targets={target};
    re4::WorldEffectCamera view;re4::RoomAreaEffects areas(data,0xffffffffu,&view);
    check(areas.quads(view).size()==1,"Static source-format forest sprite survives its original warmup");
    auto scene=plane(20000),glass=plane(2000);scene.lighting=isolated(cut);
    auto white=pixelTexture({255,255,255,255});auto mask=white;mask.rgba[3]=std::uint8_t(alpha);
    glass.textures={white,mask};auto &draw=glass.draws[0];draw.texture=0;draw.alphaTest=true;draw.alphaTexture=1;
    draw.alphaReference=0;draw.blendMode=0;draw.materialRGB={10,20,30};draw.orderType=1;draw.zMode=0;draw.sortCenter={0,0,-2000};
    re4::AppendStaticSceneLayer(scene,glass);
    auto reference=expected(cut.fog,depth,{60,150,30,255});
    if(depth>2000)reference=over(expected(cut.fog,2000,{10,20,30,255}),reference,alpha);
    for(const auto &size:{std::array<int,2>{512,448},std::array<int,2>{960,540}})
      check(near(sample(scene,size[0],size[1],0xffffffffu,&areas,&data.effects),reference,3),
          "Fogged foliage and alpha glass obey depth order on both sides of the window at native and wide resolution");
  }
}
re4::Texture solidMip(unsigned size,Color color) {
  re4::Texture image;image.width=image.height=size;
  for(unsigned i=0;i<size*size;++i)image.rgba.insert(image.rgba.end(),color.begin(),color.end());
  return image;
}
void lod(const re4::RoomLightCut &source) {
  auto scene=plane(10000);scene.lighting=isolated(source);
  auto *cut=&const_cast<re4::RoomLighting&>(*scene.lighting).cuts[0];
  cut->fog.type=0;cut->minimumLod=0;cut->maximumLod=2;cut->anisotropy=0;cut->lodBias=0;
  auto image=solidMip(64,{255,0,0,255});image.maxLOD=2;
  for(const auto &level:{solidMip(32,{0,255,0,255}),solidMip(16,{0,0,255,255})})
    image.mips.push_back({level.width,level.height,level.rgba});
  scene.textures={image};scene.draws[0].texture=0;scene.draws[0].materialRGB={255,255,255};
  scene.uv={0,0,6,0,6,7,0,0,6,7,0,7};
  const auto native=sample(scene),wide=sample(scene,960,540);
  check(near(native,{0,255,0,255},2),"Original512x448 chooses the expected authored green mip1");
  check(near(wide,native,2),"Higher-resolution cutscene retains original texture detail");
  cut->lodBias=1;check(near(sample(scene),{0,0,255,255},2),"Original LIT bias selects authored mip2");
  cut->lodBias=0;cut->maximumLod=0;
  check(near(sample(scene),{255,0,0,255},2),"Original LIT maximumLOD0 retains base texture");
  cut->minimumLod=cut->maximumLod=2;
  check(near(sample(scene),{0,0,255,255},2),"Original LIT minimumLOD2 selects stored last mip");
}
void scaledSpecular(const re4::RoomLightCut &source) {
  auto scene=plane(10000);scene.lighting=isolated(source);
  const_cast<re4::RoomLighting&>(*scene.lighting).cuts[0].fog.type=0;
  auto lookup=solidMip(64,{0,0,0,255});
  for(unsigned y=0;y<64;++y)for(unsigned x=0;x<64;++x) {
    auto *pixel=lookup.rgba.data()+4*(y*64+x);pixel[0]=x*4;pixel[1]=y*4;
  }
  scene.textures={lookup};auto &draw=scene.draws[0];draw.materialRGB={0,0,0};
  draw.specularEnabled=true;draw.specularTexture=0;draw.specularRGB={255,255,255};
  draw.specularScale=.5f;draw.specularNormalScale={10,10,10};
  // Black base still needs a white raster for source specular TEV stage.
  scene.textures.push_back(solidMip(1,{0,0,0,255}));draw.texture=1;draw.materialRGB={255,255,255};
  scene.normals.clear();for(unsigned i=0;i<6;++i)scene.normals.insert(scene.normals.end(),{.05f,.04f,.1f});
  const auto correct=sample(scene);check(near(correct,{190,75,0,255},2),
      "Scale10 source normal samples lookup UV(.75,.30) without normalizing specular texgen");
  draw.specularNormalScale={1,1,1};const auto old=sample(scene);
  check(std::abs(int(old[0])-int(correct[0]))>40 && std::abs(int(old[1])-int(correct[1]))>40,
      "Scaled source lookup differs materially from obsolete inverse-transpose-only coordinates");
}
void blockVisibility(const re4::RoomLightCut &source) {
  auto scene=plane(10000);scene.lighting=isolated(source);
  const_cast<re4::RoomLighting&>(*scene.lighting).cuts[0].fog.type=0;
  scene.draws[0].roomBlock=3;
  check(near(sample(scene,512,448,7),{0,0,0,255},0),
      "Original entry MRAM set0,1,2 suppresses cached block3 in the actual GPU submission");
  check(near(sample(scene,512,448,14),{40,80,120,255},0),
      "Moving into next source area displays block3 without recreating GPU resources");
  scene.draws[0].roomBlock=-1;
  check(near(sample(scene,512,448,0),{40,80,120,255},0),
      "Original main-room scroll objects are independent of streamed block visibility");
}
void effectBoxes(re4::OriginalEvent &event,const std::string &r100) {
  const auto archive=re4::RoomArchive::Load((std::filesystem::path(r100).parent_path()/"r120.dat").string());
  const auto *entry=archive.find("SMD");check(entry,"Original driving SMD is available");
  const re4::Smd smd(archive.copy(entry-archive.entries().data()));
  event.prepareRoomEffectModels(smd);
  check(!event.effectModels().empty(),"Actual driving event prepares its EFM04 landscape copies");
  for(const auto &[id,mesh]:event.effectModels()) {
    const auto placement=std::find_if(smd.placements.begin(),smd.placements.end(),
        [&](const auto &p){return p.work==id;});
    const auto &bin=smd.models.at(placement->bin);
    for(const auto &draw:mesh.draws)for(unsigned axis=0;axis<3;++axis) {
      check(std::abs(draw.lightVolume.center[axis]-(bin.minimum[axis]+bin.maximum[axis])*.5f)<.01f,
          "EfmSeqSet box centre matches independent original BIN bounds with root offset removed");
      check(std::abs(draw.lightVolume.size[axis]-(bin.maximum[axis]-bin.minimum[axis])*.5f)<.01f,
          "EfmSeqSet box half-sizes match independent original BIN bounds");
    }
  }
}
void eventLightVolumes(const re4::OriginalEvent &event) {
  const auto find=[&](const char *name)->const re4::GenericEventActor& {
    for(const auto &a:event.actors())if(a->name()==name)return *a;
    throw std::runtime_error("Original driving actor missing");
  };
  const auto &car=find("obm1a00");const auto box=car.lightVolume();
  check(box.type==2,"SetObj18 preserves the original car BOX light shape");
  const auto &mesh=car.mesh();
  std::array<float,3> bound{};
  for(unsigned k=0;k<3;++k)bound[k]=(mesh.minimum[k]+mesh.maximum[k])*.5f;
  check(box.axes==std::array<float,9>{},
      "Unplaced SetObj18 retains zero-filled imat, not an invented identity light box");
  check(re4::RoomLightTouches(box,{-65000,1800,-1000},3000),
      "Original zero imat accepts a distant eligible point light before any Pos packet");
  for(unsigned k=0;k<3;++k)check(std::isfinite(bound[k]),"Original BIN bounds remain finite");
  check(std::abs(car.lightSize()-std::max(mesh.maximum[0]-mesh.minimum[0],mesh.maximum[1]-mesh.minimum[1])*.5f)<.01f,
      "LightSetModel uses the source maximum X/Y half-size instead of the bound diagonal");
  const auto &source=event.lighting()->cuts[0].lights.at(7);
  check(source.type==5 && source.enableMask==4 && re4::RoomLightTouches(box,source.position,source.radius),
      "Original cut6 cabin directional light remains assigned throughout the moving-car animation");
  const auto &window=find("obm3000a");const auto sphere=window.lightVolume();
  check(sphere.type==1 && sphere.center==window.lightPosition(),
      "Pos flag0x40000000 changes a parent-following event actor to a live SPHERE light volume");
  check(sphere.size[0]==(window.mesh().maximum[0]-window.mesh().minimum[0])*.5f,
      "lightHitCheckSphere retains the original unscaled Size.x");
}
unsigned unplacedSourceBounds(const re4::OriginalEvent &event) {
  unsigned roots=0;
  for(const auto &actor:event.actors()) {
    if(!actor->visible() || actor->lightVolume().type!=2)continue;
    const bool hasPos=std::any_of(event.data().packets().begin(),event.data().packets().end(),
        [&](const auto &p){return p.type==9 && p.text(16,12)==actor->name();});
    if(hasPos)continue;
    const auto volume=actor->lightVolume();
    check(volume.axes==std::array<float,9>{},
        "Actual EVD root without Pos retains the original zero-initialized inverse light matrix");
    // In lightHitCheckBBox the zero source matrix maps every lamp to0.
    // Validate the real cut's lamps, including points beyond the identity
    // box that incorrectly excluded the house/parked-car illumination.
    for(const auto &light:event.lighting()->cuts[0].lights)
      if((light.flags&3)==3 && light.id!=4 && (light.enableMask&actor->lightMask()))
        check(re4::RoomLightTouches(volume,light.position,light.radius),
            "Original no-Pos light matrix admits every otherwise eligible authored lamp");
    ++roots;
  }
  return roots;
}
void explicitPlacementBounds(re4::OriginalEvent &event) {
  auto i=std::find_if(event.actors().begin(),event.actors().end(),
      [](const auto &actor){return actor->name()=="obm1a00";});
  check(i!=event.actors().end(),"Actual car root exists for Pos/reset regression");
  auto &car=**i;re4::SmdPlacement placement;placement.position={1000000,2000,500000};
  placement.scale={1,1,1};car.setPlacement(placement);
  check(car.lightVolume().axes!=std::array<float,9>{} &&
      !re4::RoomLightTouches(car.lightVolume(),{-65000,1800,-1000},3000),
      "Explicit Pos still rebuilds the bounded inverse matrix and rejects distant point lamps");
  event.reset();
  check(car.lightVolume().axes==std::array<float,9>{},
      "EVD restart restores no-Pos source initialization instead of retaining the previous light box");
}
}
int main(int argc,char **argv) {
  try {
    if(argc!=3)throw std::runtime_error("Usage: r100_distance_gl_check R100.dat DISC_FILES");
    Egl egl;cinematicFrame();const auto r100=re4::RoomArchive::Load(argv[1]);
    const auto lighting=re4::ReadRoomLighting(r100);
    distances(lighting->cuts[0]);billboardDistances(lighting->cuts[0]);lod(lighting->cuts[0]);scaledSpecular(lighting->cuts[0]);blockVisibility(lighting->cuts[0]);
    itemAlphaEdges(r100,lighting->cuts[0]);spriteGlassOrder(lighting->cuts[0]);
    re4::OriginalEvent drive(argv[2],"r120s00",1);unsigned tested=0,last=~0u,rootSamples=0;
    effectBoxes(drive,argv[1]);
    while(!drive.finished()) {
      if(drive.sourceCut()!=last) {
        last=drive.sourceCut();distances(drive.lighting()->cuts[0]);billboardDistances(drive.lighting()->cuts[0]);++tested;
      }
      if(drive.sourceCut()==6 && (drive.cutFrame()==30 || drive.cutFrame()==90 ||
          drive.cutFrame()==150 || drive.cutFrame()==210))eventLightVolumes(drive);
      if(drive.cutFrame()==30)rootSamples+=unplacedSourceBounds(drive);
      drive.tick30Hz();
    }
    check(tested==9 && last==8,"Every original driving cut0 through8 has its own source fog checked");
    explicitPlacementBounds(drive);
    for(const auto *name:{"r120s01","r100s03"}) {
      re4::OriginalEvent event(argv[2],name,1);
      while(!event.finished()) {
        if(event.cutFrame()==6 || event.cutFrame()==30)rootSamples+=unplacedSourceBounds(event);
        event.tick30Hz();
      }
    }
    check(rootSamples>50,"Driving, car exit and first-house roots all exercise actual source light initialization");
    std::cout<<"Original r100/r120 distance, foliage, LOD and scaled specular GLES PASS: "<<checks
        <<" checks; driving cuts="<<tested<<'\n';return 0;
  }catch(const std::exception &e){std::cerr<<"Distance GLES regression: "<<e.what()<<'\n';return 1;}
}
