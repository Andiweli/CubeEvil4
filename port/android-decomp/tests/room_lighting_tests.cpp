#include "room_lighting.h"
#include "item_rules.h"
#include "foot_shadow.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
namespace {
unsigned checks;
void check(bool value, const char *message) {
  if (!value) throw std::runtime_error(message);
  ++checks;
}
void word(std::vector<unsigned char> &b, std::size_t p, std::uint32_t value) {
  for (unsigned k = 0; k < 4; ++k) b.at(p + k) = value >> (24 - k * 8);
}
void real(std::vector<unsigned char> &b, std::size_t p, float value) {
  std::uint32_t bits; std::memcpy(&bits, &value, 4); word(b, p, bits);
}
std::vector<unsigned char> fixture() {
  std::vector<unsigned char> b(8 + 260 + 300);
  b[1] = 1; b[2] = 0x2c; b[3] = 1; word(b, 4, 8);
  b[8] = 27; b[9] = 27; b[10] = 27; b[11] = 128;
  word(b, 12, 1); word(b, 16, 2); real(b, 20, -7000); real(b, 24, 175000);
  b[28] = 113; b[29] = 108; b[30] = 90; b[31] = 128;
  b[260] = 35; b[264] = 15;
  b[268] = 7; b[269] = 5; b[271] = 0x51;
  real(b, 284, 170000); real(b, 292, 1.2f);
  real(b, 312, 1000); real(b, 316, 10000); real(b, 320, 100000);
  return b;
}
void rejects(std::vector<unsigned char> b) {
  bool failed = false;
  try { re4::RoomLighting lighting(b); } catch (const std::exception &) { failed = true; }
  check(failed, "Malformed LIT must be rejected before GPU upload");
}
}
int main() {
  try {
    const re4::RoomLighting lighting(fixture());
    const auto &cut = lighting.cuts.at(0);
    check(cut.screenAmbient[0] == 27 && cut.actorAmbient[0] == 35 && cut.effectAmbient[0] == 15,
          "Screen, actor and effect ambient words remain independent");
    check(cut.lights.size() == 1 && cut.lights[0].type == 5 && cut.lights[0].enableMask == 0x51,
          "GC directory and light enable mask decoded bytewise");
    check(cut.fog.start == -7000 && std::abs(re4::RoomFogAmount(cut.fog, 0) - 7000.f / 182000) < 1e-7,
          "Negative original fog start gives a near-field veil");
    check(std::abs(re4::RoomFogAmount(cut.fog, 84000) - .5f) < 1e-7 &&
          re4::RoomFogAmount(cut.fog, 200000) == 1, "Linear depth and far saturation");
    const auto registers=re4::RoomFogParameters(cut.fog);
    check(registers.b==4194738 && registers.shift==2 && !registers.orthographic,
          "GXSetFog stores its original truncated B mantissa and exponent");
    float previousFog=0;
    for(float depth:{100.f,1000.f,10000.f,30000.f,70000.f,150000.f,500000.f}) {
      const auto raster=std::uint32_t((1000000.f/999900.f-100000000.f/(999900.f*depth))*16777216.f);
      const auto amount=re4::RoomFogRasterAmount(cut.fog,registers,raster);
      check(amount>=previousFog && std::abs(amount*256-std::round(amount*256))<1.e-6,
            "Original 24-bit depth gives monotonic fog quantized to256 blend steps");previousFog=amount;
    }
    auto environment=fixture();environment[8+64]=1;environment[8+65]=2;
    environment[8+242]=1;environment[8+243]=3;environment[8+244]=2;real(environment,8+248,.75f);
    real(environment,8+68,.6f);environment[8+72]=50;
    environment[8+47]=131;environment[8+240]=1;environment[8+241]=254;
    environment[8+245]=1;environment[8+246]=27;environment[8+247]=5;
    word(environment,8+40,65499);environment[8+45]=5;environment[8+46]=1;
    word(environment,8+24,2);real(environment,8+28,100);real(environment,8+32,50000);
    const auto env=re4::RoomLighting(environment).cuts[0];
    check(env.minimumLod==1 && env.maximumLod==3 && env.anisotropy==2 && env.lodBias==.75f,
          "Original LIT texture LOD fields242/243/244/248 are independent of contrast and ambient");
    auto badSampler=environment;badSampler[8+242]=10;badSampler[8+243]=10;badSampler[8+244]=3;
    const auto repairedSampler=re4::RoomLighting(badSampler).cuts[0];
    check(repairedSampler.minimumLod==0 && repairedSampler.maximumLod==5 && repairedSampler.anisotropy==0,
          "Original setMipmap replaces out-of-range sampler bytes with0/5/0");
    check(env.tevScale==std::array<unsigned char,2>{1,2} && env.farRatio==.6f && env.fogFrames==50,
          "Original ENV TEV scales, culling ratio and Hokan count are independent");
    check(env.mirrorFog.type==2 && env.mirrorFog.start==100 && env.mirrorFog.end==50000,
          "Mirror fog uses its own ENV record");
    check(env.blurRate==131 && env.blurType==1 && env.blurPower==-2 && env.contrast==std::array<unsigned char,3>{1,27,5},
          "Original Filter00 blur, signed zoom power and contrast bytes retained");
    check(env.focusDepth==65499 && env.focusLevel==5 && env.focusMode==1,
          "Original ENV Filter01 depth, level and mode are read independently");
    check(re4::RoomVisibilityFar(env.fog,env.farRatio)==env.fog.end*(1-env.farRatio)+1,
          "Original visibility far plane follows fog and its authored ratio");
    check(re4::RoomVisibilityFar(re4::RoomFog{},1)==1000000,
          "No-fog cut ignores unused far-ratio bytes");
    const std::array<float,3> eye{},right{1,0,0},up{0,1,0},look{0,0,1};
    check(re4::RoomModelVisible({0,0,500},10,eye,right,up,look,1,4.f/3,1000),
          "Foreground model inside original view remains visible");
    check(!re4::RoomModelVisible({0,0,1100},10,eye,right,up,look,1,4.f/3,1000),
          "Far forest model outside the view is culled before drawing");
    check(re4::RoomModelVisible({0,0,1005},10,eye,right,up,look,1,4.f/3,1000),
          "Model intersecting far plane is retained by the source sphere test");
    check(!re4::RoomModelVisible({0,0,50},10,eye,right,up,look,1,4.f/3,1000),
          "Model before original near plane is culled");
    check(!re4::RoomModelVisible({1000,0,500},10,eye,right,up,look,1,4.f/3,1000),
          "Model outside the source side planes is culled");
    auto unusedEnvironment=environment;word(unusedEnvironment,8+8,0);word(unusedEnvironment,8+24,0);
    real(unusedEnvironment,8+28,std::numeric_limits<float>::quiet_NaN());
    real(unusedEnvironment,8+68,std::numeric_limits<float>::quiet_NaN());
    unusedEnvironment[8+64]=255;unusedEnvironment[2]=0x28;
    const auto upgraded=re4::RoomLighting(unusedEnvironment).cuts[0];
    check(upgraded.tevScale[0]==2 && !upgraded.fogFrames && !upgraded.farRatio && !upgraded.mirrorFog.start,
          "Source versionUp ignores unused no-fog fields, clamps TEV and clears old-format Hokan");
    std::vector<unsigned char> inactiveCamera(80,0);inactiveCamera[0]='B';inactiveCamera[1]='4';inactiveCamera[2]='0';inactiveCamera[3]='3';inactiveCamera[5]=1;
    word(inactiveCamera,24,32);real(inactiveCamera,36,std::numeric_limits<float>::quiet_NaN());
    re4::RoomLighting cameraFixture(environment);cameraFixture.readCameraAreas(inactiveCamera);
    check(cameraFixture.areas.empty(),"Disabled source camera areas never interpret unused editor-name bytes as floats");
    auto stored=fixture();stored[268]=0;
    const auto storedLight=re4::RoomLighting(stored).cuts[0].lights[0];
    check(storedLight.position==cut.lights[0].position && storedLight.intensity==1.2f,
          "Inactive screen lights retain their authored data for sscrnLightCreate");
    real(stored,312,std::numeric_limits<float>::quiet_NaN());
    check(re4::RoomLighting(stored).cuts[0].lights[0].position[0]==0,
          "Unused stored light fields do not discard an otherwise valid camera cut");
    re4::RoomLighting zones(environment);zones.cuts.resize(3,env);zones.present={true,true,true};
    const std::vector<std::array<float,3>> square{{0,0,0},{0,0,10},{10,0,10},{10,0,0}};
    zones.areas={{1,1,3,1,0,-100,300,square},{1,2,0x23,1,0,-100,300,square}};
    check(zones.selectCut({5,0,5},0,1)==2,"Priority CAM area wins over the retained normal area");
    zones.areas[1].attributes|=4;
    check(zones.selectCut({5,0,5},0,1)==1,"Disabled CAM area never changes lighting");
    zones.areas[0].number=2;zones.areas[1].attributes=3;
    check(zones.selectCut({5,0,5},0,2)==2,"Normal overlap keeps the previous light area");
    check(zones.selectCut({20,0,5},0)==0 && zones.selectCut({5,200,5},0)==0,
          "Outside and upper-exclusive CAM height fall back to Cut0");
    zones.areas[0].attributes=0x43;zones.areas[1].flags=0;
    check(zones.selectCut({5,0,5},3.1415927f)==0 && zones.selectCut({5,0,5},0)==2,
          "CAM direction gate uses Leon heading and the source 135-degree limit");
    re4::RoomLightTransition transition;transition.select(zones,0,0);
    zones.cuts[1].fog.start=1000;zones.cuts[1].fog.color={220,210,200,128};zones.cuts[1].fogFrames=2;
    transition.select(zones,1,0);
    check(transition.remaining==2 && transition.fog.start==-7000,"Cut change starts Hokan without an immediate fog jump");
    transition.select(zones,1,1./30);
    check(transition.remaining==1 && transition.fog.start==-3000 && transition.fog.color[0]==166,
          "First Hokan tick decrements first and truncates colour to a source byte");
    transition.select(zones,1,1./30);
    check(transition.remaining==0 && transition.fog.start==1000 && transition.fog.color[0]==220,
          "Final Hokan tick reaches the exact target");
    re4::RoomLightVolume volume;volume.size={100,1,100};
    check(!re4::RoomLightTouches(volume,{0,50,0},5) && re4::RoomLightTouches(volume,{105,0,0},5),
          "Source thin-box light selection rejects the enclosing sphere false positive and includes touching faces");
    volume.type=0;volume.size={10,100,0};
    check(re4::RoomLightTouches(volume,{0,100,0},1) && !re4::RoomLightTouches(volume,{50,0,0},1),
          "Cylinder tests the two authored endpoint spheres, not their enclosing sphere");
    check(re4::RoomLightTouches(volume,{10000,0,0},0),"Source zero-radius light reaches every volume");
    auto b = fixture(); b.resize(300); rejects(b);
    b = fixture(); word(b, 4, 4); rejects(b);
    b = fixture(); b[2] = 0x2d; rejects(b);
    b = fixture(); b[2]=0x29; b[3]=0; b[271]=1; b[296]=0xbf;
    const re4::RoomLighting legacy(b);
    check(legacy.sourceVersion==0x29 && legacy.cuts[0].lights[0].enableMask==0x41 &&
          legacy.cuts[0].lights[0].parent==0 && legacy.normalizedParents==1,
          "cLit::versionUp refreshes the legacy class mask and calcParent detaches invalid parents");
    const auto one=fixture();
    b.assign(12+one.size()-8,0); b[1]=2; b[2]=0x2c; b[3]=1; word(b,4,12);
    std::copy(one.begin()+8,one.end(),b.begin()+12);
    const re4::RoomLighting sparse(b);
    check(sparse.cuts.size()==2 && sparse.present[0] && !sparse.present[1] &&
          sparse.cuts[0].lights.size()==1 && sparse.unavailableCuts.empty(),
          "Zero source offset retains a missing cut without shifting camera indices");
    auto directional=fixture();word(directional,324,1);
    check(re4::RoomLighting(directional).cuts[0].lights[0].directionFlags==1,
          "Parallel camera-space Flag is a BE32 word, not its zero high byte");
    auto custom=fixture();custom[269]=4;
    for(unsigned k=0;k<6;++k)real(custom,324+k*4,float(k+1));
    const auto raw=re4::RoomLighting(custom).cuts[0].lights[0].customAttenuation;
    check(raw==std::array<float,6>{1,2,3,4,5,6},
          "Type4 preserves original A0,A1,A2,K0,K1,K2 at cLightWork+0x38");
    auto shadows=fixture();shadows[269]=1;shadows[270]=4;shadows[271]=1;
    shadows[288]=255;shadows[289]=255;shadows[290]=255;shadows[291]=128;
    shadows[378]=5;shadows[379]=0xff; // Shadow work kind5 and signed -1 offset.
    const auto shadowLight=re4::RoomLighting(shadows).cuts[0].lights[0];
    check(shadowLight.shadow.kind==5&&shadowLight.shadow.texture==255,
          "Id4 shadow metadata is independent of the Type1 light function");
    auto shadowCut=re4::RoomLighting(shadows).cuts[0];
    shadowCut.lights[0].radius=0;
    std::vector<std::array<float,12>> parts(26);
    for(auto &m:parts){m[0]=m[5]=m[10]=1;m[7]=600;}
    parts[21][3]=-100;parts[25][3]=100;
    const auto blobs=re4::BuildPlayerFootShadows(shadowCut,parts,{0,0,0},0,0);
    check(blobs.size()==12&&blobs.front().position==std::array<float,3>{-100,40,0}&&blobs.front().size==300,
          "Original pl_fsd_dat joints, linked subdivisions and signed height offset reproduced");
    check(re4::BuildPlayerFootShadows(shadowCut,parts,{0,0,0},0,255).empty(),
          "Source Shd_color0xff disables all foot-shadow quads");
    word(b,8,unsigned(b.size())); b.insert(b.end(),one.begin()+8,one.end());
    real(b,12+one.size()-8+12,std::numeric_limits<float>::quiet_NaN());
    const re4::RoomLighting unusedInvalid(b);
    check(unusedInvalid.present[0] && !unusedInvalid.present[1] &&
          unusedInvalid.unavailableCuts==std::vector<unsigned>{1},
          "Unusable noninitial camera cut is reported and never substituted into the valid initial cut");
    b = fixture(); word(b, 12, 2); rejects(b);
    b = fixture(); real(b, 20, std::numeric_limits<float>::quiet_NaN()); rejects(b);
    b = fixture(); word(b, 16, 3); rejects(b);
    b = fixture(); real(b, 24, -8000); rejects(b);
    b = fixture(); b[269] = 8; rejects(b);
    const auto handgunAmmo = re4::OriginalItemInfo(4), shotgun = re4::OriginalItemInfo(0x2c);
    check(handgunAmmo.type == 2 && handgunAmmo.defNum == 10 && handgunAmmo.maxNum == 50,
          "Original handgun stack default and maximum");
    check(shotgun.type == 1 && shotgun.defNum == 0 && re4::OriginalItemInfo(6).defNum == 1,
          "A weapon's zero default is distinct from a recovery item's default");
    check(re4::OriginalItemInfo(0x79).defNum == 5 && re4::OriginalItemInfo(0xffff).type == 7,
          "Original itemInfo fallthrough and map fallback retained");
    check(re4::OriginalMoneyValue(0x78,100)==1000 && re4::OriginalMoneyValue(0x79,0)==3000 &&
          re4::OriginalMoneyValue(0x71,0)==5000 && !re4::OriginalMoneyValue(4,10),
          "Currency values use get() defaults independently of itemInfo()");
    std::cout << "M29 lighting/item contracts PASS: " << checks << '\n';
  } catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
