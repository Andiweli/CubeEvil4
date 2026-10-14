#include "subscreen_data.h"
#include "inventory_case.h"
#include "sat.h"
#include <cmath>
#include <algorithm>
#include <cstdlib>
#include <cassert>
#include <cstring>
#include <fstream>
#include <iostream>
using namespace re4;
namespace {
unsigned checks = 0;
void checkAt(bool value,unsigned line) { ++checks;if(!value)std::cerr<<"Subscreen check "<<checks<<" failed at line "<<line<<'\n';assert(value); }
#define check(value) checkAt((value),__LINE__)
void word(std::vector<std::uint8_t> &b, unsigned p, unsigned value) {
  for (unsigned i = 0; i < 4; ++i) b[p + i] = value >> ((3 - i) * 8);
}
void real(std::vector<std::uint8_t> &b, unsigned p, float value) {
  unsigned bits; std::memcpy(&bits, &value, 4); word(b, p, bits);
}
template<class F> void reject(F fn) {
  bool rejected = false; try { fn(); } catch (const std::exception &) { rejected = true; }
  check(rejected);
}
std::vector<std::uint8_t> read(const std::string &name) {
  std::ifstream stream(name, std::ios::binary);
  return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}
}
int main(int argc, char **argv) {
  std::vector<std::uint8_t> arc(80);
  word(arc,0,3); word(arc,16,32); word(arc,20,48); word(arc,24,48);
  arc[32] = 7; arc[48] = 9;
  check(CopySubscreenEntry(arc,4).size() == 16);
  check(CopySubscreenEntry(arc,5).size() == 32);
  check(CopySubscreenEntry(arc,5) == CopySubscreenEntry(arc,6));
  reject([&] { CopySubscreenEntry(arc,3); });
  reject([&] { CopySubscreenEntry(arc,7); });
  auto broken = arc; word(broken,16,12);
  reject([&] { CopySubscreenEntry(broken,4); });
  broken = arc; word(broken,20,81);
  reject([&] { CopySubscreenEntry(broken,5); });
  std::vector<std::uint8_t> layout(8 + 0x8c);
  layout[0]='2'; layout[1]='.'; layout[2]='0'; layout[3]='0'; layout[5]=1;
  layout[8+4]=2; layout[8+5]=8; layout[8+7]=255;
  layout[8+11]=12; layout[8+12]=1; layout[8+0x6e]=255;
  for (unsigned i=0;i<4;++i) layout[8+0x58+i]=255;
  real(layout,8+0x14,30); real(layout,8+0x18,20);
  real(layout,8+0x50,80); real(layout,8+0x54,40);
  auto q=DecodeSubscreenLayout(layout).at(0);
  check(q.position[0]==30 && q.position[1]==20 && q.texture==12);
  check(q.vertices[0]==0 && q.vertices[2]==80 && q.vertices[5]==-40);
  check(q.color[3]==255);
  auto unusedColor=layout;unusedColor.resize(layout.size()+48);
  word(unusedColor,8+0x74,unsigned(layout.size()));
  word(unusedColor,8+0x5c,0xff00ffff); // unused col1 with an empty color curve
  check(DecodeSubscreenLayout(unusedColor).at(0).color==q.color);
  auto unmaskedLayout=layout;unmaskedLayout[8+0x6e]=0;
  check(DecodeSubscreenLayout(unmaskedLayout).at(0).mask==255);
  unmaskedLayout[8+0x6f]=1;
  check(DecodeSubscreenLayout(unmaskedLayout).at(0).mask==0);
  // The source ID path animates from an offscreen pos0 to its authored end;
  // a static menu must use the same end point rather than dropping path0.
  auto pathLayout=layout;pathLayout.resize(layout.size()+80);
  unsigned pathAt=unsigned(layout.size()),curveAt=pathAt+32;
  word(pathLayout,8+0x74,pathAt);word(pathLayout,8+0x74+8,curveAt);
  pathLayout[pathAt]=1;pathLayout[pathAt+7]=2;
  real(pathLayout,pathAt+8,-575);real(pathLayout,pathAt+12,110);
  real(pathLayout,pathAt+20,-95);real(pathLayout,pathAt+24,110);
  word(pathLayout,curveAt,2);real(pathLayout,curveAt+4,0);real(pathLayout,curveAt+8,0);
  real(pathLayout,curveAt+20,7);real(pathLayout,curveAt+24,1);
  const auto settled=DecodeSubscreenLayout(pathLayout).at(0);
  check(settled.position[0]==510 && settled.position[1]==20);
  auto malformedPath=pathLayout;malformedPath[pathAt+7]=65;
  reject([&] { DecodeSubscreenLayout(malformedPath); });
  auto glyphs=DecodeSubscreenGlyphs({0,0x81,3,6,2,0x83,1});
  check(glyphs.size()==2 && glyphs[0].glyph==1 && glyphs[1].glyph==3);
  check(glyphs[1].line==1 && glyphs[1].color==2);
  const auto choices=DecodeSubscreenGlyphs({0,7,0x81,3,7,0x82,1},true);
  check(choices.size()==4 && choices[0].color==99 && choices[0].line==0 && choices[2].color==99 && choices[2].line==1);
  check(DecodeSubscreenGlyphs({0,7,0x81,3,7,0x82,1}).size()==2);
  reject([] { DecodeSubscreenGlyphs({0,2,5,1}); });
  reject([] { DecodeSubscreenGlyphs({0,6}); });
  for(unsigned file=1;file<=31;++file) check(OriginalFileNumber(OriginalFileItem(file))==file);
  check(OriginalFileNumber(0x21)==0 && OriginalFileItem(0)==0);
  check(OriginalFilePageInfo(1,4).picturePath=="ss/Eng/f01e.tpl");
  check(OriginalFilePageInfo(16,4).picture==1);
  check(OriginalFilePageInfo(16,5).picture==2);
  check(OriginalFilePageInfo(16,4,"jpn").picture==2);
  reject([] { OriginalFilePageInfo(30,0); });
  reject([] { OriginalFilePageInfo(1,5); });
  check(OriginalKeyItemTextureFrame(0x3b)==2 && OriginalKeyItemTextureFrame(0x21)==0);
  auto profile=OriginalItemExamProfile(0x23);
  check(profile.authored && profile.scale==1.5f && profile.rotationDegrees[0]==-30);
  check(profile.fovy==30 && profile.light==0);
  if(argc==2) {
    const std::string files=argv[1];
    for(unsigned i=0;i<5;++i) {
      auto art=LoadSubscreenArt(files,static_cast<SubscreenArtKind>(i));
      check(!art.textures.empty() && !art.quads.empty());
      check(art.layoutJson().find("\"width\":640")!=std::string::npos);
      std::cout<<"Original art "<<i<<": "<<art.textures.size()<<" frames, "<<art.quads.size()<<" units\n";
    }
    const auto common=LoadSubscreenArt(files,SubscreenArtKind::Common);
    check(common.texture(7,9)!=nullptr); // original ten digit frames
    bool hasTabs=false,hasMoney=false,settledBoard=false;
    for(const auto &unit:common.quads) {
      hasTabs|=unit.group==11;hasMoney|=unit.group==7;
      if(unit.group==9 && unit.mark==254)settledBoard=unit.position[0]==-95 && unit.position[1]==110;
    }
    check(hasTabs && hasMoney && settledBoard);
    for(const auto &size:std::vector<std::array<unsigned,2>>{{10,6},{12,7},{14,8},{15,8}}) {
      const auto board=LoadOriginalCaseBoardImage(files,size[0],size[1]);
      check(board.width==640 && board.height==480);
      unsigned covered=0;for(std::size_t p=3;p<board.rgba.size();p+=4)covered+=board.rgba[p]!=0;
      check(covered>20000 && covered<640*480);
    }
    reject([&] { LoadOriginalCaseBoardImage(files,11,6); });
    for(const auto id:{0x21u,0x23u,0x2cu}) {
      const auto portrait=LoadOriginalMenuPortraitImage(files,id);
      check(portrait.width==640 && portrait.height==480);
      unsigned covered=0,left=0;
      for(unsigned y=0;y<480;++y)for(unsigned x=0;x<640;++x)
        if(portrait.rgba[(y*640+x)*4+3]){++covered;if(x<400)++left;}
      check(covered>10000 && left==0); // no skin explosion into the case
    }
    const auto font=LoadOriginalMenuFont(files);
    check(!font.sheets.empty() && font.bearings.size()>100);
    for(const auto message:{0x11u,0x12u,0x13u,0x14u,0x15u}) {
      const auto text=LoadOriginalPickupMessage(files,message==0x14?0x78:4,message,10,0);
      check(text.width==512 && text.height==384);
      unsigned coverage=0;bool sourceBaseline=true;
      for(unsigned y=0;y<384;++y)for(unsigned x=0;x<512;++x)
        if(text.rgba[(y*512+x)*4+3]){++coverage;sourceBaseline&=y>=240 && y<322;}
      check(coverage>100 && sourceBaseline);
      if(message<=0x13)check(text.rgba!=LoadOriginalPickupMessage(files,4,message,10,1).rgba);
    }
    unsigned pages=0,glyphCount=0;
    for(unsigned file=1;file<=29;++file) {
      const auto count=OriginalFilePageInfo(file,0).pages;
      for(unsigned page=0;page<count;++page) {
        const auto text=LoadOriginalFileGlyphs(files,file,page);
        check(!text.empty()); glyphCount+=text.size(); ++pages;
        const auto picture=LoadOriginalFilePicture(files,file,page);
        check(picture.width>0 && picture.height>0);
        const auto composed=LoadOriginalFilePage(files,OriginalFileItem(file),page);
        check(composed.width==640 && composed.height==480);
      }
    }
    std::cout<<"Original files: "<<pages<<" pages, "<<glyphCount<<" glyphs\n";
    for(unsigned light=0;light<5;++light) {
      auto source=LoadOriginalExamLighting(files,light);
      check(source->present.at(0) && source->cuts.at(0).lights.size()>=3);
    }
    check(!LoadOriginalExamLayout(files).empty());
    const auto map=read(files+"/ss/Cmn/map_obj1.dat");
    check(!ReadOriginalVillageMapRoom(map,0x100).empty());
    check(!ReadOriginalVillageMapRoom(map,0x101).empty());
    unsigned icons=0;
    for(unsigned id=0;id<256;++id)if(OriginalItemFootprint(id).width) {
      const auto icon=LoadOriginalItemIcon(files,id);
      const auto shape=OriginalItemFootprint(id);
      check(icon.width==shape.width*48 && icon.height==shape.height*48);
      unsigned pixels=0;
      for(std::size_t p=3;p<icon.rgba.size();p+=4)pixels+=icon.rgba[p]!=0;
      check(pixels>10);++icons;
      const auto model=LoadOriginalItemScene(files,id);
      check(model.triangles>0 && model.fallbackTriangles==0);
      const auto specular=LoadOriginalItemSpecular(files,id);
      check(specular.size()==model.draws.size());
      for (const auto &part : specular) if (part.enabled) {
        check(part.image.width && part.image.height && part.scale>0);
        check(part.type<=1 && part.rgb[0]>=0 && part.rgb[0]<=1);
      }
    }
    std::cout<<"Original case/examine: "<<icons<<" source models and transparent icons\n";
    unsigned names=0;
    for (unsigned id=0;id<256;++id) {
      const auto name=LoadOriginalItemName(files,id);
      check(name.width>1 && name.width<=2049 && name.height==32);
      unsigned pixels=0;
      for (std::size_t p=3;p<name.rgba.size();p+=4) pixels+=name.rgba[p]!=0;
      check(pixels>10); ++names;
    }
    check(std::abs(LoadOriginalItemSpecular(files,0x23).at(0).scale-.065f)<.000001f);
    std::cout<<"Original item-name glyph images: "<<names<<"\n";
    // Area room counts/boundary rows are the exact upstream mapDataInit_St*
    // lists, rather than a guess from room IDs or collision adjacency.
    const unsigned areaCases[][3] = {
      {0x101,1,22},{0x200,2,27},{0x21d,3,11},{0x21a,4,2},
      {0x300,5,1},{0x306,6,5},{0x307,7,8},{0x310,8,5},
      {0x315,9,13},{0x329,10,5},{0x400,11,1},{0x402,12,1},
      {0x403,13,1},{0x404,14,1},{0x405,15,1},{0x406,16,1},
      {0x40a,17,8}};
    for (const auto &source : areaCases) {
      const auto area = OriginalMapSnapshotJson(files,source[0],{0,0,0},0);
      check(area.find("\"scope\":\"area\"") != std::string::npos);
      check(area.find("\"area\":" + std::to_string(source[1]) + ',') != std::string::npos);
      check(area.find("\"roomCount\":" + std::to_string(source[2]) + ',') != std::string::npos);
      check(area.find("\"progressKnown\":false") != std::string::npos);
      check(area.find("\"geometry\":\"BIN\"") != std::string::npos);
      check(area.find("\"paths\":[{\"texture\":") != std::string::npos);
      check(area.find("\"current\":true") != std::string::npos);
    }
    const auto villageBoundary = OriginalMapSnapshotJson(files,0x200,{0,0,0},0,1);
    check(villageBoundary.find("\"area\":1,") != std::string::npos);
    check(villageBoundary.find("\"roomCount\":22,") != std::string::npos);
    check(villageBoundary.find("\"stageKnown\":true") != std::string::npos);
    const auto castleBoundary = OriginalMapSnapshotJson(files,0x307,{0,0,0},0);
    check(castleBoundary.find("\"id\":774,") != std::string::npos); // source r306 duplicate.
    reject([&] { OriginalMapSnapshotJson(files,0x302,{0,0,0},0); });
    reject([&] { OriginalMapSnapshotJson(files,0x101,{0,0,0},0,5); });
    // An actual room-world floor centroid must map to its paired map-space
    // centroid even after adding the other area rooms to paths.
    const auto r101 = CopySubscreenEntry(map,0x0c);
    const unsigned entries = (unsigned(r101[0]) << 24) | (unsigned(r101[1]) << 16) |
                             (unsigned(r101[2]) << 8) | r101[3];
    const Sat mini(CopySubscreenEntry(r101,entries + 2));
    const Sat collision(CopySubscreenEntry(r101,entries + 3));
    std::array<double,3> position{}, expected{};
    for (auto vertex : collision.polygons.at(0).vertices) for (unsigned d = 0; d < 3; ++d) {
      position[d] += collision.vertices.at(vertex)[d] / 3.;
      expected[d] += mini.vertices.at(vertex)[d] / 3.;
    }
    const auto result = OriginalMapSnapshotJson(files,0x101,position,0);
    check(result.find("\"markerValid\":true") != std::string::npos);
    const auto start = result.find("\"player\":[") + 10;
    char *next = nullptr;
    const auto markerX = std::strtod(result.c_str() + start,&next);
    const auto markerZ = std::strtod(next + 1,nullptr);
    check(std::abs(markerX - expected[0]) < .001);
    check(std::abs(markerZ - expected[2]) < .001);
    ScenarioState progress;std::map<std::uint16_t,std::uint8_t> passed;
    auto explored=OriginalMapSnapshotJson(files,0x100,{0,0,0},0,-1,&progress,&passed);
    check(explored.find("\"camera\":[0,0,10000,55]")!=std::string::npos);
    check(explored.find("\"colorIndex\":4,\"color\":[255,255,255,0]")!=std::string::npos);
    check(explored.find("\"colorIndex\":2,")==std::string::npos);
    passed[0x101]=0x80;
    explored=OriginalMapSnapshotJson(files,0x100,{0,0,0},0,0,&progress,&passed);
    check(explored.find("\"colorIndex\":1,\"color\":[135,135,135,255]")!=std::string::npos);
    passed[0x104]=0x80;progress.scenario.set(28);
    explored=OriginalMapSnapshotJson(files,0x100,{0,0,0},0,0,&progress,&passed);
    check(explored.find("\"colorIndex\":3,\"color\":[35,20,20,255]")!=std::string::npos);
    progress.scenario.set(29);
    explored=OriginalMapSnapshotJson(files,0x100,{0,0,0},0,0,&progress,&passed);
    check(explored.find("\"colorIndex\":1,")==std::string::npos);
    check(explored.find("\"colorIndex\":0,\"color\":[255,255,255,255]")!=std::string::npos);
    const auto mapFrame=DecodeSubscreenLayout(CopySubscreenEntry(read(files+"/ss/Eng/ss_map.dat"),7));
    const auto viewport=std::find_if(mapFrame.begin(),mapFrame.end(),[](const auto &u){return u.mark==0xfe;});
    check(viewport!=mapFrame.end());
    const auto viewAt=explored.find("\"viewport\":[")+12;
    const double viewX=std::strtod(explored.c_str()+viewAt,&next),viewY=std::strtod(next+1,&next);
    const double viewW=std::strtod(next+1,&next),viewH=std::strtod(next+1,nullptr);
    check(std::abs(viewX-320-viewport->position[0])<.001 && std::abs(viewY-240+viewport->position[1])<.001);
    check(std::abs(viewH-viewW*.75)<.001 && viewW+.001>=std::abs(viewport->vertices[2]-viewport->vertices[0]));
    std::cout << "Original area maps: 17 area lists, 113 source room rows\n";
  }
  std::cout<<"Subscreen checks: "<<checks<<" PASS\n";
}
