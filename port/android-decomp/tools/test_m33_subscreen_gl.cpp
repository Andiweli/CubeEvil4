// Actual original IDM items through the production independent GLES3 preview.
// Assets stay in the user's extracted disc; the test never packages game data.
#include "subscreen_data.h"
#include "subscreen_item_gl.h"
#include "subscreen_portrait_gl.h"
#include <set>
#include "inventory_case.h"
#include "room_lighting_gl.h"
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <array>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
namespace {
int width=640,height=480;
unsigned checks{};
void check(bool value,const char *message) {
  if(!value)throw std::runtime_error(message);
  ++checks;
}
struct Egl {
  EGLDisplay display=EGL_NO_DISPLAY;
  EGLContext context=EGL_NO_CONTEXT;
  EGLSurface surface=EGL_NO_SURFACE;
  Egl() {
    setenv("EGL_PLATFORM","surfaceless",1);
    display=eglGetDisplay(EGL_DEFAULT_DISPLAY);
    EGLint major{},minor{};
    check(eglInitialize(display,&major,&minor),"Item EGL initialize");
    check(eglBindAPI(EGL_OPENGL_ES_API),"Item EGL API");
    const EGLint attributes[]{EGL_SURFACE_TYPE,EGL_PBUFFER_BIT,
      EGL_RENDERABLE_TYPE,EGL_OPENGL_ES3_BIT,EGL_RED_SIZE,8,EGL_GREEN_SIZE,8,
      EGL_BLUE_SIZE,8,EGL_ALPHA_SIZE,8,EGL_DEPTH_SIZE,24,EGL_NONE};
    EGLConfig config{};EGLint count{};
    check(eglChooseConfig(display,attributes,&config,1,&count)&&count,
      "Item EGL config");
    const EGLint ca[]{EGL_CONTEXT_CLIENT_VERSION,3,EGL_NONE};
    context=eglCreateContext(display,config,EGL_NO_CONTEXT,ca);
    const EGLint sa[]{EGL_WIDTH,width,EGL_HEIGHT,height,EGL_NONE};
    surface=eglCreatePbufferSurface(display,config,sa);
    check(context!=EGL_NO_CONTEXT&&surface!=EGL_NO_SURFACE&&
      eglMakeCurrent(display,surface,surface,context),"Item EGL current");
  }
  ~Egl() {
    if(display!=EGL_NO_DISPLAY) {
      eglMakeCurrent(display,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);
      if(surface!=EGL_NO_SURFACE)eglDestroySurface(display,surface);
      if(context!=EGL_NO_CONTEXT)eglDestroyContext(display,context);
      eglTerminate(display);
    }
  }
};
std::vector<unsigned char> pixels() {
  std::vector<unsigned char> result(width*height*4);
  glReadPixels(0,0,width,height,GL_RGBA,GL_UNSIGNED_BYTE,result.data());
  check(glGetError()==GL_NO_ERROR,"Item framebuffer read has no GL error");
  return result;
}
void background() {
  glDisable(GL_SCISSOR_TEST);glDisable(GL_BLEND);glViewport(0,0,width,height);
  glDepthMask(GL_TRUE);glClearColor(.2f,.1f,.3f,1);
  glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
}
unsigned shapePixels(const std::vector<unsigned char>&image) {
  unsigned result{};
  for(int y=110;y<370;++y)for(int x=100;x<540;++x) {
    const auto p=(y*width+x)*4;
    if(image[p]!=9||image[p+1]!=9||image[p+2]!=6)++result;
  }
  return result;
}
void contained(const std::vector<unsigned char>&image,unsigned id) {
  for(int y=110;y<370;++y)for(int x=100;x<540;++x)
    if(x<102||x>=538||y<112||y>=368) {
      const auto p=(y*width+x)*4;
      if(image[p]!=9||image[p+1]!=9||image[p+2]!=6)
        throw std::runtime_error("Original item "+std::to_string(id)+" silhouette is clipped by its preview frame");
    }
  ++checks;
}
void boundary(const std::vector<unsigned char>&before,
              const std::vector<unsigned char>&after) {
  for(int y=0;y<height;++y)for(int x=0;x<width;++x)
    if(x<100||x>=540||y<110||y>=370) {
      const auto p=(y*width+x)*4;
      for(unsigned k=0;k<4;++k)
        if(before[p+k]!=after[p+k])throw std::runtime_error("Item draw changed world pixels outside its preview");
    }
  ++checks;
}
void save(const std::string &path,const std::vector<unsigned char>&image) {
  std::ofstream out(path,std::ios::binary);out<<"P6\n"<<width<<' '<<height<<"\n255\n";
  for(int y=height-1;y>=0;--y)for(int x=0;x<width;++x)
    out.write(reinterpret_cast<const char*>(image.data()+(y*width+x)*4),3);
  check(bool(out),"Item preview export");
}
void portrait(const std::string &files,const std::string &prefix) {
  for(const unsigned weapon:{0x23u,0x2cu,0u}) {
    auto model=re4::LoadOriginalMenuPortrait(files,weapon);
    background();const auto frame=pixels();const auto atmosphere=re4::RoomAtmosphere();
    const auto image=re4::RenderSubscreenPortraitGl(model,640,480);
    check(image.width==640&&image.height==480,"Original SS camera renders at source size");
    unsigned opaque=0;for(std::size_t p=0;p<image.rgba.size();p+=4)opaque+=image.rgba[p+3]==255;
    check(opaque>10000,"Source SS Leon/body/head/hands/weapon produce opaque portrait pixels");
    check(pixels()==frame&&re4::RoomAtmosphere().data==atmosphere.data&&re4::RoomAtmosphere().eye==atmosphere.eye,
        "Portrait render preserves paused world pixels and lighting");
    std::set<int> masks;for(const auto &draw:model.scene.draws)if(draw.alphaTest)masks.insert(draw.alphaTexture);
    unsigned changed=0;
    for(unsigned texture=0;texture<model.scene.textures.size();++texture)if(!masks.count(int(texture))) {
      auto &rgba=model.scene.textures[texture].rgba;for(std::size_t p=3;p<rgba.size();p+=4){changed+=rgba[p]!=0;rgba[p]=0;}
    }
    check(changed>0,"Actual SS material contains unused stored texture alpha");
    check(re4::RenderSubscreenPortraitGl(model,640,480).rgba==image.rgba,
        "Unused opaque-material texture alpha cannot create holes in Leon face/body");
    const auto wide=re4::RenderSubscreenPortraitGl(model,1920,1080);
    check(wide.width==1920&&wide.height==1080&&pixels()==frame,"Native screen resolution portrait preserves the room framebuffer");
    if(!prefix.empty()) {
      auto bottomUp=image.rgba;const auto stride=std::size_t(width)*4;
      for(int y=0;y<height/2;++y)std::swap_ranges(bottomUp.begin()+y*stride,bottomUp.begin()+(y+1)*stride,bottomUp.begin()+(height-1-y)*stride);
      save(prefix+"-leon-"+std::to_string(weapon)+".ppm",bottomUp);
    }
  }
}

void spinel(const std::string &files,const std::string &prefix,const re4::RoomArchive *room) {
  // Original name/message87 is Spinel; this treasure is not one of the58
  // attache-case pieces exercised by the generic preview loop.
  const auto scene=re4::LoadOriginalItemScene(files,0x57,room==nullptr,room);
  const auto profile=re4::OriginalItemExamProfile(0x57,false);
  const auto light=re4::LoadOriginalExamLighting(files,profile.light);
  const auto specular=re4::LoadOriginalItemSpecular(files,0x57,room==nullptr,room);
  check(!scene.draws.empty(),"Spinel contains the actual original gem mesh");
  std::set<int> masks;
  for(const auto &draw:scene.draws){check(draw.cullMode==0,"Spinel inherits original cModel CullMode0");if(draw.alphaTest)masks.insert(draw.alphaTexture);}
  auto spareAlpha=scene;unsigned replaced=0;
  for(unsigned i=0;i<spareAlpha.textures.size();++i)if(!masks.count(int(i)))
    for(std::size_t p=3;p<spareAlpha.textures[i].rgba.size();p+=4){replaced+=spareAlpha.textures[i].rgba[p]!=0;spareAlpha.textures[i].rgba[p]=0;}
  check(replaced>0,"Spinel base TPL contains stored alpha that is not a material mask");
  std::vector<std::vector<unsigned char>> reference;
  for(unsigned pass=0;pass<2;++pass) {
    background();re4::CreateSubscreenItemGl(pass?spareAlpha:scene,profile.rotationDegrees,
      profile.scale,0x57,light,profile.pickupAxis,false,{145,-35},specular);
    for(unsigned tick=0;tick<120;++tick) {
      re4::DrawSubscreenItemGl(width,height,tick?1.0/30:0);
      auto frame=pixels();check(shapePixels(frame)>200,"Spinel remains visible through its complete360-degree rotation");
      contained(frame,0x57);re4::DrawSubscreenItemGl(width,height,0);
      check(pixels()==frame,"Spinel facets are stable when the same angle is redrawn");
      if(!pass){reference.push_back(frame);if(!prefix.empty() && tick%15==0)save(prefix+"-spinel-"+std::to_string(tick)+".ppm",frame);}
      else check(frame==reference[tick],"Unused base texture alpha cannot cause rotating Spinel holes or flicker");
    }
    re4::DestroySubscreenItemGl();
  }
}

}
int main(int argc,char **argv) {
  try {
    if(argc<2||argc>4)throw std::runtime_error("Usage: m33_subscreen_gl_check DISC1_FILES [IMAGE_PREFIX] [R100_ARCHIVE]");
    const std::string files=argv[1],prefix=argc>=3?argv[2]:"";
    std::unique_ptr<re4::RoomArchive> pickupRoom;
    if(argc==4)pickupRoom=std::make_unique<re4::RoomArchive>(re4::RoomArchive::Load(argv[3]));
    unsigned items{},mirrored{},specularVisible{};
    std::array<float,2> frameY{145,-35};
    const auto frameArt=re4::LoadOriginalExamArt(files);
    for(const auto &unit:frameArt.quads) {
      if(unit.mark==0xf2)frameY[0]=unit.position[1];
      if(unit.mark==0xf3)frameY[1]=unit.position[1];
    }
    check(frameArt.quads.size()==12 && frameArt.texture(97) && frameArt.texture(25),
      "Pickup contains the original CORE_EXAMINE_ID panel and rounded message frame");
    const auto specimen=re4::LoadOriginalItemScene(files,0x21);
    const auto specimenProfile=re4::OriginalItemExamProfile(0x21);
    const auto specimenLight=re4::LoadOriginalExamLighting(files,specimenProfile.light);
    const auto specimenSpecular=re4::LoadOriginalItemSpecular(files,0x21);
    std::vector<unsigned char> recreatedImage;
    {
      Egl egl;portrait(files,prefix);spinel(files,prefix,pickupRoom.get());
      for(const auto size:{std::array<unsigned,2>{10,6},{11,7},{12,8},{15,8}}) {
        const auto board=re4::LoadOriginalCaseBoardImage(files,size[0],size[1]);unsigned opaque=0;
        for(std::size_t p=3;p<board.rgba.size();p+=4)opaque+=board.rgba[p]>0;
        check(board.width==640 && board.height==480 && opaque>10000,"Each original inventory upgrade resolves the matching case mesh and source projection");
      }
      for(unsigned id=0;id<256;++id) {
        if(!re4::OriginalItemFootprint(id).width)continue;
        const auto scene=re4::LoadOriginalItemScene(files,id);
        const auto profile=re4::OriginalItemExamProfile(id);
        const auto light=re4::LoadOriginalExamLighting(files,profile.light);
        const auto specular=re4::LoadOriginalItemSpecular(files,id);
        check(scene.triangles>0&&!scene.draws.empty(),"Source item contains original geometry");
        check(scene.fallbackTriangles==0,"Source item resolves every original TPL texture");
        for(const auto &t:scene.textures)mirrored+=t.wrapS==2||t.wrapT==2;
        auto &room=re4::RoomAtmosphere();room={};room.data=light;
        room.eye={123,456,789};room.look={0,-1,0};
        const auto oldData=room.data;
        const auto oldEye=room.eye,oldLook=room.look;
        background();const auto before=pixels();
        re4::CreateSubscreenItemGl(scene,profile.rotationDegrees,profile.scale,id,light,
          profile.pickupAxis,false,frameY,specular);
        check(glGetError()==GL_NO_ERROR,"Production item shader and texture creation succeeds");
        re4::DrawSubscreenItemGl(width,height,0);
        const auto image=pixels();
        check(shapePixels(image)>20,"Actual original item is visible in its GLES framebuffer");
        if(!prefix.empty() && id==0)save(prefix+"-0.ppm",image);
        contained(image,id);
        boundary(before,image);
        check(room.data==oldData&&room.eye==oldEye&&room.look==oldLook,
          "Item preview restores the world atmosphere owner and camera");
        GLint viewport[4]{};glGetIntegerv(GL_VIEWPORT,viewport);
        check(viewport[0]==0&&viewport[1]==0&&viewport[2]==width&&viewport[3]==height,
          "Item preview restores full game viewport");
        re4::DrawSubscreenItemGl(width,height,0);
        check(pixels()==image,"Paused zero-time preview retains exact pixels");
        re4::DrawSubscreenItemGl(width,height,1.0/30);
        check(glGetError()==GL_NO_ERROR,"Authored 30Hz item spin renders without GL errors");
        contained(pixels(),id);
        if(!prefix.empty()&&(id==0x21||id==0x23||id==0x17||id==0x2c||id==0x06))
          save(prefix+"-"+std::to_string(id)+".ppm",image);
        re4::DestroySubscreenItemGl();
        check(glGetError()==GL_NO_ERROR,"Independent item GL resources cleanly destroy");
        if(!specular.empty()) {
          background();
          re4::CreateSubscreenItemGl(scene,profile.rotationDegrees,profile.scale,id,light,
            profile.pickupAxis,false,frameY,{});
          re4::DrawSubscreenItemGl(width,height,0);
          if(pixels()!=image)++specularVisible;
          re4::DestroySubscreenItemGl();
          check(glGetError()==GL_NO_ERROR,"Reference without item reflection has no GL errors");
        }
        ++items;
      }
      check(items==58,"Every original attache-case item gets a real GLES preview");
      check(specularVisible>0,"Original section reflection changes visible GLES framebuffer pixels");
      // Item pickup uses the original full EFB snapshot and source camera,
      // rather than the bounded inventory examination window.
      const auto ammo=re4::LoadOriginalItemScene(files,0x04);
      const auto coins=re4::LoadOriginalItemScene(files,0x78,false);
      const auto coinProfile=re4::OriginalItemExamProfile(0x78,false);
      background();const auto beforeCoins=pixels();
      re4::SetSubscreenItemViewport({0,0,1,1});
      re4::CreateSubscreenItemGl(coins,coinProfile.rotationDegrees,coinProfile.scale,0x78,
        re4::LoadOriginalExamLighting(files,coinProfile.light),coinProfile.pickupAxis,false,frameY,re4::LoadOriginalItemSpecular(files,0x78,false));
      re4::DrawSubscreenItemGl(width,height,0);
      const auto coinPixels=pixels();unsigned coinChanges=0;
      for(std::size_t p=0;p<coinPixels.size();p+=4)coinChanges+=coinPixels[p]!=beforeCoins[p] || coinPixels[p+1]!=beforeCoins[p+1] || coinPixels[p+2]!=beforeCoins[p+2];
      check(coinChanges>100,"Money pickup uses original Cmn/itm78 BIN/TPL and visibly renders without nonexistent idm078");
      re4::DestroySubscreenItemGl();
      const auto ammoProfile=re4::OriginalItemExamProfile(0x04);
      re4::SetSubscreenItemViewport({0,0,1,1});background();const auto pickupBackground=pixels();
      re4::CreateSubscreenItemGl(ammo,ammoProfile.rotationDegrees,ammoProfile.scale,0x04,
        re4::LoadOriginalExamLighting(files,ammoProfile.light),ammoProfile.pickupAxis,false,frameY,
        re4::LoadOriginalItemSpecular(files,0x04));
      re4::DrawSubscreenItemGl(width,height,0);const auto pickup=pixels();
      unsigned pickupShape=0;
      for(std::size_t p=0;p<pickup.size();p+=4)
        pickupShape+=pickup[p]!=pickupBackground[p]||pickup[p+1]!=pickupBackground[p+1]||pickup[p+2]!=pickupBackground[p+2];
      check(pickupShape>100,"Fullscreen original pickup camera displays the genuine ammo model");
      check(std::equal(pickup.begin(),pickup.begin()+4,pickupBackground.begin()),
        "Original item pickup preserves the frozen world background at the screen edge");
      glClearColor(.7f,.5f,.1f,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
      re4::DrawSubscreenItemGl(width,height,0);
      check(pixels()==pickup,"Pickup EFB background remains frozen even if the incoming world framebuffer changes");
      if(!prefix.empty())save(prefix+"-pickup-ammo.ppm",pickup);
      re4::DestroySubscreenItemGl();
      background();
      re4::CreateSubscreenItemGl(ammo,ammoProfile.rotationDegrees,ammoProfile.scale,0x04,
        re4::LoadOriginalExamLighting(files,ammoProfile.light),ammoProfile.pickupAxis,false,frameY,
        re4::LoadOriginalItemSpecular(files,0x04),frameArt);
      re4::DrawSubscreenItemGl(width,height,0);const auto framedPickup=pixels();
      unsigned frameChanges=0,uncoveredItem=0;
      for(std::size_t p=0;p<pickup.size();p+=4) {
        const bool item=pickup[p]!=pickupBackground[p]||pickup[p+1]!=pickupBackground[p+1]||pickup[p+2]!=pickupBackground[p+2];
        const bool changed=framedPickup[p]!=pickup[p]||framedPickup[p+1]!=pickup[p+1]||framedPickup[p+2]!=pickup[p+2];
        frameChanges+=!item && changed;uncoveredItem+=item && !changed;
      }
      check(frameChanges>100000,"Original frame textures shade the large panel and separate bottom message area");
      check(uncoveredItem>500,"OT0x13 frame is behind OT0x14 and preserves the lit ammo model pixels");
      glClearColor(.7f,.5f,.1f,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
      re4::DrawSubscreenItemGl(width,height,0);
      check(pixels()==framedPickup,"Repeated pickup redraw reconstructs the frame from frozen EFB without compounding darkening");
      if(!prefix.empty())save(prefix+"-pickup-original-frame.ppm",framedPickup);
      re4::DestroySubscreenItemGl();
      re4::SetSubscreenItemViewport({100.f/640,110.f/480,440.f/640,260.f/480});
      background();re4::CreateSubscreenItemGl(specimen,specimenProfile.rotationDegrees,
        specimenProfile.scale,0x21,specimenLight,specimenProfile.inventoryAxis,true,frameY,specimenSpecular);
      re4::DrawSubscreenItemGl(width,height,0);recreatedImage=pixels();
      re4::DrawSubscreenItemGl(width,height,0);
      check(pixels()==recreatedImage,"Manual examination is frozen with zero elapsed time");
      re4::DrawSubscreenItemGl(width,height,1.0/30,1,.5f);
      const auto rotated=pixels();
      check(rotated!=recreatedImage,"Actual source manual camera orbit changes original item pixels");
      contained(rotated,0x21);
      re4::DrawSubscreenItemGl(width,height,0,1,.5f);
      check(pixels()==rotated,"Zero-time manual orbit preserves exact rotated pixels");
      re4::ForgetSubscreenItemGl();
    }
    {
      Egl egl;background();
      re4::CreateSubscreenItemGl(specimen,specimenProfile.rotationDegrees,
        specimenProfile.scale,0x21,specimenLight,specimenProfile.inventoryAxis,true,frameY,specimenSpecular);
      re4::DrawSubscreenItemGl(width,height,0);
      check(pixels()==recreatedImage,"New EGL context recreates original item with exact pixels");
      re4::DestroySubscreenItemGl();
    }
    {
      // Match the user's1331x571 reference, then exercise narrow and tall
      // windows with the same production frame/model projection.
      for(const auto size:{std::array<int,2>{1331,571},{1280,720},{480,640}}) {
        width=size[0];height=size[1];Egl egl;
        re4::SetSubscreenItemViewport({0,0,1,1});
        for(unsigned id:{0x04u,0x78u}) {
          const auto scene=re4::LoadOriginalItemScene(files,id,false);
          const auto profile=re4::OriginalItemExamProfile(id,false);
          const auto lighting=re4::LoadOriginalExamLighting(files,profile.light);
          const auto specular=re4::LoadOriginalItemSpecular(files,id,false);
          background();const auto room=pixels();
          re4::CreateSubscreenItemGl(scene,profile.rotationDegrees,profile.scale,id,
              lighting,profile.pickupAxis,false,frameY,specular);
          re4::DrawSubscreenItemGl(width,height,0);const auto item=pixels();
          re4::DestroySubscreenItemGl();background();
          re4::CreateSubscreenItemGl(scene,profile.rotationDegrees,profile.scale,id,
              lighting,profile.pickupAxis,false,frameY,specular,frameArt);
          re4::DrawSubscreenItemGl(width,height,0);const auto framed=pixels();
          unsigned visible=0;
          for(std::size_t p=0;p<item.size();p+=4)
            if((item[p]!=room[p] || item[p+1]!=room[p+1] || item[p+2]!=room[p+2]) &&
                std::equal(item.begin()+p,item.begin()+p+3,framed.begin()+p))++visible;
          check(visible>100,"Original ammo and money remain in front of the frame on wide and tall displays");
          re4::DrawSubscreenItemGl(width,height,0);
          check(pixels()==framed,"Adapted original requester reconstructs frozen pixels without feedback");
          if(!prefix.empty() && width==1331 && id==0x04)save(prefix+"-pickup-wide.ppm",framed);
          re4::DestroySubscreenItemGl();
        }
      }
      if(!prefix.empty()) {
        const auto message=re4::LoadOriginalPickupMessage(files,0x04,0x13,10,0);
        std::ofstream out(prefix+"-pickup-text.rgba",std::ios::binary);
        out.write(reinterpret_cast<const char*>(message.rgba.data()),std::streamsize(message.rgba.size()));
        check(message.width==512 && message.height==384 && bool(out),"Original pickup font source remains512x384");
      }
    }
    std::cout<<"M33 original item preview GLES PASS: "<<checks<<" checks, "<<items
      <<" actual IDM scenes; mirrored texture records "<<mirrored
      <<"; source reflection visible on "<<specularVisible<<" items\n";
    return 0;
  }catch(const std::exception &e) {std::cerr<<e.what()<<'\n';return 1;}
}
