// Real production JNI/GLES regression with separately supplied disc assets.
#define RE4_M27_ENTRY previous_m27_main
#include "test_m27_runtime.cpp"
#undef RE4_M27_ENTRY
#include <filesystem>
namespace m30 {
using namespace m27_test;
std::array<unsigned char,4> texel(GLuint texture,int x,int y) {
 GLint old=0;glGetIntegerv(GL_FRAMEBUFFER_BINDING,&old);GLuint f;glGenFramebuffers(1,&f);glBindFramebuffer(GL_FRAMEBUFFER,f);
 glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,texture,0);
 require(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE,"Uploaded material readable as texture");
 std::array<unsigned char,4> p{};glReadPixels(x,y,1,1,GL_RGBA,GL_UNSIGNED_BYTE,p.data());
 glBindFramebuffer(GL_FRAMEBUFFER,GLuint(old));glDeleteFramebuffers(1,&f);return p;
}
}
int main(int argc,char **argv){using namespace m30;try{
 if(argc!=10)throw std::runtime_error("Usage: m30_runtime_check R101 PL00 WEP01 roominfo CORE EM12 emleon00 R100 OUTPUT_PREFIX");
 Egl egl;Jni jni;jlong session=3001;
 Java_com_ast_re4decomp_NativeBridge_environmentData(&jni.env,nullptr,string(argv[5]));
 Java_com_ast_re4decomp_NativeBridge_configureEnemy(&jni.env,nullptr,string(argv[6]),string(argv[7]));
 const auto create=[&](const char *path,const char *name){Java_com_ast_re4decomp_NativeBridge_createScene(&jni.env,nullptr,string(path),string(argv[2]),string(argv[3]),string(argv[4]),string(name),session);frozenPixels(jni);require(javaException.empty()&&leon&&actorGpuReady&&walking,"Direct room has native actor and renderer");};
 create(argv[1],"r101");unpause(jni);
 auto &rack=roomInteractions->racks()[1];require(rack.placement.slot==15,"Source cabinet selected");
 auto feet=point(rack.placement,-950,0,0);require(walkProbe->place(feet),"Cabinet push side has real floor");
 leon->setPlacement(walkProbe->position(),rack.placement.rotation[1]+1.5707964f);
 re4::SceneFollowHeading(leon->heading());re4::SceneSeedFollowView(0,0);controls(jni);calls.clear();const auto original=frozenPixels(jni);save(std::string(argv[9])+"-cabinet-before.ppm");
 auto scene=re4::BuildStaticScene(*previewSmd,-1,previewCommon.get());re4::AttachSceneTextures(*previewSmd,scene,previewCommon.get());if(previewSmx)re4::ApplySceneSmx(*previewSmx,scene);if(previewEnvironment)re4::AttachSceneEnvironment(*previewEnvironment,scene);re4::AppendRoomObjectScene(scene,*previewObjects);
 for(const auto &d:scene.draws)if(d.roomObjectSlot==15 && d.alphaTest){
  for(const auto &c:calls)if(c.first<=GLint(d.first)&&c.first+c.count>=GLint(d.first+d.count)){
   auto base=texel(GLuint(c.texture),4,32),mask=texel(GLuint(c.alphaTexture),4,32);
   std::cout<<"Cabinet uploaded material tex "<<c.texture<<" mask "<<c.alphaTexture<<" base="<<unsigned(base[0])<<","<<unsigned(base[1])<<","<<unsigned(base[2])<<","<<unsigned(base[3])<<" alpha="<<unsigned(mask[3])<<" units "<<c.imageUnit<<","<<c.alphaUnit<<'\n';
   require(c.imageUnit==0&&c.alphaUnit==1&&base[0]<10&&mask[3]>0,"Cabinet black shadow material and original mask are separate uploaded textures");
  }
 }
 unsigned shadows=0,handles=0;
 for(const auto &d:scene.draws)if(d.roomObjectSlot==15&&d.alphaTest){
  if(d.orderType==1){require(d.zMode==1&&d.count==6,"Only the original cabinet ground quad is translucent and cannot write depth");omittedDraws.push_back({GLint(d.first),GLint(d.count)});++shadows;}
  else {require(d.zMode==0&&d.count==24,"Original solid cabinet handles retain depth writes");++handles;}
 }
 require(shadows==1&&handles==1,"Source shadow and solid handles have separate passes");
 const auto bareFloor=frozenPixels(jni);omittedDraws.clear();
 const auto shadowFloor=frozenPixels(jni);unsigned darker=0,brighter=0;
 for(std::size_t p=0;p<bareFloor.size();p+=4){int delta=0;for(unsigned k=0;k<3;++k)delta+=int(shadowFloor[p+k])-int(bareFloor[p+k]);if(delta < -3)++darker;if(delta>3)++brighter;}
 require(darker>20&&brighter==0,"Real cabinet shadow darkens existing wood floor without a pale fog-colored rectangle");
 Java_com_ast_re4decomp_NativeBridge_input(&jni.env,nullptr,0,0,1,0,0,0,0);
 ticks(jni,30);require(leon->pushActive(),"Physical LS forward automatically begins source furniture action");
 const auto position=rack.placement.position;require(std::hypot(position[0]-rack.base.position[0],position[2]-rack.base.position[2])>200,"Original cabinet physically moves in JNI path");
 pause(jni);auto frozen=frozenPixels(jni);auto *owner=roomInteractions.get();auto action=leon->action();auto bones=leon->rig().bones();
 egl.fresh();create(argv[1],"r101");require(owner==roomInteractions.get()&&rack.placement.position==position&&leon->action()==action&&leon->rig().bones()==bones&&pixelChanges(frozen,frozenPixels(jni))==0,"Mid-push pause and actual EGL loss preserve owner, model, animation and pixels");
 unpause(jni);controls(jni);ticks(jni,30);require(!leon->pushActive(),"Releasing physical LS finishes source furniture release");save(std::string(argv[9])+"-cabinet-after.ppm");
 // Streaming probe draw never emits the old floor patch while Leon is present.
 calls.clear();frozenPixels(jni);for(const auto &c:calls)require(!(c.mode==GL_TRIANGLES&&c.first==0&&c.count==36&&c.lighting==0&&c.fog==0&&c.textured==0),"Normal Leon render has no old yellow contact fan");
 // Real gun laser under the actual LIT atmosphere: inspect the submitted pass.
 controls(jni,0,1);ticks(jni,15);calls.clear();frozenPixels(jni);bool laser=false;
 for(const auto &c:calls)if(c.mode==GL_LINES&&c.count==2){laser=true;require(c.lighting==0&&c.fog==0,"Source laser is unfogged and unlit in production GLES");}
 require(laser,"Physical LT aims and submits the original source laser");
 save(std::string(argv[9])+"-laser.ppm");
 controls(jni);ticks(jni,45);bool jump=false;
 for(std::size_t i=0;i<previewSat->polygons.size()&&!jump;++i){
  const auto &poly=previewSat->polygons[i];if(!(poly.attributes&0x100010))continue;
  const auto normal=previewSat->normals[poly.normal];if(std::abs(normal[1])>.5)continue;
  const auto tri=collisionWorld->triangle(i);
  for(double distance:{400.,600.,800.}){
   re4::CollisionVec p{};for(const auto &corner:tri)for(unsigned k=0;k<3;++k)p[k]+=corner[k]/3;
   for(unsigned k=0;k<3;++k)p[k]+=normal[k]*distance;
   const auto floor=collisionWorld->ground(p,1000,1500);if(!floor)continue;
   p[1]=floor->point[1];if(!walkProbe->place(p))continue;
   leon->setPlacement(walkProbe->position(),float(std::atan2(-normal[0],-normal[2])));
   if(const auto offer=leon->jumpOffer(*walkProbe)){leon->beginJump(*walkProbe,*offer);jump=true;break;}
  }
 }
 require(jump,"Actual r101 SAT edge starts an original fall action");ticks(jni,18);
 require(leon->jumpActive(),"Source fall is active during render check");calls.clear();frozenPixels(jni);
 for(const auto &c:calls)require(!(c.mode==GL_TRIANGLES&&c.first==0&&c.count==36&&c.lighting==0&&c.fog==0&&c.textured==0),"Actual fall render contains no yellow floor contact fan");
 for(unsigned i=0;i<300&&leon->jumpActive();++i)ticks(jni,1);
 require(!leon->jumpActive()&&leon->life()==1200,"Fall finishes on actual floor with infinite test health");
 // Verify the existing rollback using a valid main room whose required
 // sibling blocks are absent, independently of Java's earlier hash gate.
 const auto missing=std::filesystem::temp_directory_path()/
     ("re4-m30-missing-blocks-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
 std::filesystem::create_directory(missing);const auto mainCopy=missing/"r100.dat";
 std::filesystem::copy_file(argv[8],mainCopy);
 const auto previous=frozenPixels(jni);auto *previousObjects=roomInteractions.get();
 requestRoom(0x100,0,{-99685,-484,-1343},2.246f);const auto rejected=token(jni,"r100");
 Java_com_ast_re4decomp_NativeBridge_finishRoomRequest(&jni.env,nullptr,jlong(rejected),string(mainCopy.c_str()),string(""));
 const auto afterFailure=frozenPixels(jni);std::filesystem::remove_all(missing);
 require(roomRequest.token==0&&activeRoomId==0x101&&previousObjects==roomInteractions.get()&&pixelChanges(previous,afterFailure)==0,"Missing r100 blocks reject activation and preserve previous owners and exact render");
 ++session;create(argv[8],"r100");require(activeRoomId==0x100&&previewLighting&&previewEnvironment->sky.size()==1,"r100 original atmosphere and sky are installed");
 require(previewBlocks&&previewBlocks->count==5&&previewBlocks->mesh.triangles>20000&&previewBlocks->mesh.included>50,"r100 contains the original five block instance sets rather than background alone");
 std::cout<<"r100 "<<previewBlocks->describe()<<'\n';
 require(std::abs(walkProbe->position()[0]+99685)<1&&std::abs(walkProbe->position()[2]+1343)<1,"r100 starts at original playable post-intro placement");
 pause(jni);calls.clear();auto r100Image=frozenPixels(jni);save(std::string(argv[9])+"-r100.ppm");
 require(glGetError()==GL_NO_ERROR,"r100 real materials, sky, water and atmosphere render cleanly");
 egl.fresh();create(argv[8],"r100");require(pixelChanges(r100Image,frozenPixels(jni))==0,"r100 render and start survive context recreation");
 std::cout<<"M30 actual JNI/GLES regressions passed: "<<checks<<'\n';return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
