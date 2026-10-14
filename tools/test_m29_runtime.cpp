// Actual production JNI/GL: r101 fog, ITA pickup, jump A edge, pause and
// context recreation. Original assets are supplied locally, never bundled.
#define main previous_bridge_test_main
#include "test_android_bridge_gl.cpp"
#undef main
#include "../app/src/main/cpp/room_lighting_gl.h"
namespace m29 {
using namespace bridge_test;
void fogPixels(const re4::RoomArchive &room) {
  re4::RoomAtmosphere().data = re4::ReadRoomLighting(room);
  auto &state = re4::RoomAtmosphere(); state.eye = {0,0,0}; state.look = {0,0,1};
  const auto vs = re4::RoomAtmosphereShader(R"GLSL(#version 300 es
precision highp float;
void main(){vec2 p=gl_VertexID==0?vec2(-1,-1):gl_VertexID==1?vec2(3,-1):vec2(-1,3);gl_Position=vec4(p,0,1);}
)GLSL", true);
  const auto fs = re4::RoomAtmosphereShader(R"GLSL(#version 300 es
precision highp float;
uniform float sampleDepth;
out vec4 outColor;
void main(){outColor=fogRoomColor(vec4(1,1,1,1),vec3(0,0,sampleDepth));}
)GLSL", false);
  const auto v = shader(GL_VERTEX_SHADER, vs.c_str()), f = shader(GL_FRAGMENT_SHADER, fs.c_str());
  const auto p = glCreateProgram(); glAttachShader(p,v); glAttachShader(p,f); glLinkProgram(p);
  GLint linked = 0; glGetProgramiv(p,GL_LINK_STATUS,&linked); require(linked, "Production atmosphere shader links");
  GLuint a; glGenVertexArrays(1,&a); glBindVertexArray(a);
  glViewport(0,0,960,540); glDisable(GL_DEPTH_TEST); glDisable(GL_BLEND); glUseProgram(p);
  re4::BindRoomFog(p);
  for (float z : {0.f,80000.f,200000.f}) {
    glUniform1f(glGetUniformLocation(p,"sampleDepth"),z); glDrawArrays(GL_TRIANGLES,0,3);
    std::array<unsigned char,4> pixel{}; glReadPixels(480,270,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel.data());
    // Independent linear GX oracle, using the actual r101 disc constants.
    const auto amount = std::clamp((double(z)+7074.842285)/182105.029785,0.,1.);
    const int fog[] = {113,108,90};
    for (unsigned k=0;k<3;++k)
      require(std::abs(int(pixel[k])-int(std::lround(255*(1-amount)+fog[k]*amount)))<=1,
              "GPU fog matches the original negative-start GX linear depth");
    require(pixel[3]==255 && glGetError()==GL_NO_ERROR,"Fog retains alpha and clean GLES state");
  }
  glDeleteVertexArrays(1,&a); glDeleteShader(v); glDeleteShader(f); glDeleteProgram(p);
}
bool placeForItem(std::size_t index) {
  const auto &item = roomPickups->items().at(index);
  constexpr double pi=3.14159265358979323846;
  for (double radius : {1000.,900.,600.,300.}) for (unsigned n=0;n<24;++n) {
    const auto angle=n*2*pi/24;
    auto feet=item.position; feet[0]+=std::sin(angle)*radius; feet[2]+=std::cos(angle)*radius;
    const auto floor=collisionWorld->ground(feet,600,2500); if(!floor) continue;
    feet[1]=floor->point[1]; if(!walkProbe->place(feet)) continue;
    const auto heading=std::atan2(item.position[0]-feet[0],item.position[2]-feet[2]);
    leon->setPlacement(walkProbe->position(),float(heading));
    auto center=leon->feet(); center[1]+=250; auto front=center;
    front[0]+=std::sin(heading)*550; front[2]+=std::cos(heading)*550;
    if(const auto wall=collisionWorld->line(center,front,re4::SatSubset::All,0)) front=wall->point;
    if(roomPickups->target(leon->feet(),heading,front)!=index || leon->jumpOffer(*walkProbe) ||
       roomInteractions->doorOffer(leon->feet(),heading) ||
       roomLadders->offer(*leon,*walkProbe,front) || roomFlow->select(leon->feet(),heading,false,false,front).door) continue;
    re4::SceneFollowHeading(float(heading)); return true;
  }
  return false;
}
bool placeForJump() {
  for(std::size_t i=0;i<previewSat->polygons.size();++i) {
    const auto &p=previewSat->polygons[i]; if(!(p.attributes&0x100010)) continue;
    const auto normal=previewSat->normals[p.normal]; if(std::abs(normal[1])>.5) continue;
    auto tri=collisionWorld->triangle(i); re4::CollisionVec feet{};
    for(const auto &corner:tri) for(unsigned k=0;k<3;++k) feet[k]+=corner[k]/3;
    for(unsigned k=0;k<3;++k) feet[k]+=normal[k]*400;
    const auto floor=collisionWorld->ground(feet,1000,1500); if(!floor) continue;
    feet[1]=floor->point[1]; if(!walkProbe->place(feet)) continue;
    leon->setPlacement(walkProbe->position(),float(std::atan2(-normal[0],-normal[2])));
    if(leon->jumpOffer(*walkProbe) && !roomLadders->offer(*leon,*walkProbe)) return true;
  }
  return false;
}
}
int main(int argc,char **argv) {
  using namespace bridge_test;
  try {
    if(argc!=9) throw std::runtime_error("Usage: m29_runtime_check R101.dat PL00 WEP01 roominfo CORE EM12 emleon00 OUTPUT_PREFIX");
    Egl egl; Jni jni;
    m29::fogPixels(re4::RoomArchive::Load(argv[1]));
    Java_com_ast_re4decomp_NativeBridge_environmentData(&jni.env,nullptr,string(argv[5]));
    Java_com_ast_re4decomp_NativeBridge_configureEnemy(&jni.env,nullptr,string(argv[6]),string(argv[7]));
    auto create=[&] {
      Java_com_ast_re4decomp_NativeBridge_createScene(&jni.env,nullptr,string(argv[1]),string(argv[2]),string(argv[3]),string(argv[4]),string("r101"),2901);
      frozenPixels(jni);
      require(javaException.empty() && actorGpuReady && roomPickups && previewLighting && previewLooseItems,
              "Production r101 pickups, LIT and player owners ready");
    };
    create(); require(leon->unlimitedHealth() && leon->life()==1200,"Default Android scene uses unlimited health");
    require(previewEnvironment->sky.size()==1 && previewEnvironment->sky[0].work==0,"r101 selects the original cloud-dome work0");
    save(std::string(argv[8])+"-village.ppm");
    std::size_t item=roomPickups->items().size();
    for(std::size_t n=0;n<roomPickups->items().size();++n)
      if(roomPickups->items()[n].id==4 && m29::placeForItem(n)) { item=n; break; }
    require(item<roomPickups->items().size(),"Actual loose handgun ammo is reachable on genuine floor with clear EAT");
    controls(jni); const auto before=frozenPixels(jni); const auto reserve=leon->reserve();
    save(std::string(argv[8])+"-pickup-before.ppm");
    const auto count=roomPickups->items()[item].count, flag=roomPickups->items()[item].autoFlag;
    require(hudInfo.find("Handgun Ammo pick up")!=std::string::npos && roomPickups->visible(int(item)),"Native HUD offers the active original ITA model");
    controls(jni,re4::Bottom); frozenPixels(jni);
    require(pendingPickup.token && paused && leon->itemExamActive() && roomPickups->visible(int(item)),
            "Physical A opens original item examination without prematurely taking the ammo");
    Java_com_ast_re4decomp_NativeBridge_answerPickup(&jni.env,nullptr,jlong(pendingPickup.token),JNI_TRUE);
    const auto after=frozenPixels(jni);
    require(!pendingPickup.token && inventoryCase.count(4)==reserve+count && scenarioState.carriedItems.empty(),
            "Confirmed authored ammo has one case owner, with no duplicate loose-item carrier");
    save(std::string(argv[8])+"-pickup-after.ppm");
    require(leon->reserve()==reserve+count && !roomPickups->visible(int(item)) && scenarioState.room(0x101).items.test(flag),
            "Physical A transfers source ammunition, hides model and sets native taken flag");
    std::cerr << "Pickup pixel changes: " << pixelChanges(before,after) << '\n';
    require(pixelChanges(before,after)>10,"Original ITM geometry disappears visibly after pickup");
    frozenPixels(jni); require(leon->reserve()==reserve+count,"Held A never duplicates pickup");
    controls(jni); frozenPixels(jni); storeInteractionState();
    require(scenarioState.room(0x101).items.test(flag),"Interior state writeback cannot erase taken flags");
    require(m29::placeForJump(),"Actual roof offers a SAT-validated jump");
    controls(jni); frozenPixels(jni);
    require(hudInfo.find("Jump down")!=std::string::npos,"Actual roof HUD offers A jump");
    controls(jni,re4::Bottom); tick(jni); require(leon->jumpActive(),"Physical A starts original fall animation");
    controls(jni); ticks(jni,18); pause(jni);
    const auto feet=leon->feet(); const auto pose=leon->rig().bones(); const auto tickCount=leon->jumpState().ticks;
    const auto view=re4::SaveSceneView(); const auto *owner=roomPickups.get(); const auto *lighting=previewLighting.get();
    const auto reserveAfter=leon->reserve();
    frozenPixels(jni); require(leon->feet()==feet && leon->rig().bones()==pose,"Paused production jump freezes exact pose");
    egl.release(); egl.fresh(); create();
    require(paused && roomPickups.get()==owner && previewLighting.get()==lighting &&
            leon->jumpState().ticks==tickCount && leon->feet()==feet && leon->rig().bones()==pose &&
            leon->reserve()==reserveAfter && !roomPickups->visible(int(item)) &&
            sameFollowState(view.followCamera,re4::SceneFollowState()),
            "Lost EGL context retains jump, pause, source flags, inventory, lighting and camera owners");
    controls(jni,re4::Start); frozenPixels(jni); controls(jni); frozenPixels(jni);
    unsigned limit=0; while(leon->jumpActive() && limit++<260) tick(jni);
    require(!leon->jumpActive() && !leon->jumpState().failed && std::abs(leon->feet()[1])<100 && leon->life()==1200,
            "Resumed native jump lands on original ground with unlimited HP");
    require(scenarioState.room(0x101).items.test(flag),"World ticks preserve pickup flags after landing");
    save(std::string(argv[8])+"-landing.ppm");
    std::cout << "M29 production JNI/GLES PASS: " << checks << '\n';
  } catch(const std::exception &e) { std::cerr<<e.what()<<'\n'; return 1; }
}
