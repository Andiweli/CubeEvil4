#include "startup_data.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <stdexcept>

int main(int argc,char **argv) {
  try {
    if(argc!=2&&argc!=3)throw std::runtime_error("Usage: title_assets_check DISC_FILES [CARD_FIXTURE_DIRECTORY]");
    const re4::TitleData title(argv[1],1);
    unsigned checks=0;
    for(unsigned group:{6u,7u,8u,9u,10u,11u,13u,1009u,1010u,1011u,1012u,1013u,1014u,2006u,2007u,2008u,2009u,6031u,6032u,6038u,7005u,7006u,7007u}) {
      const auto json=title.layoutJson(group);
      if(json.find("\"units\":[")==std::string::npos)throw std::runtime_error("Missing original layout");
      ++checks;
    }
    if(title.layoutJson(6).find("[-15,")==std::string::npos)throw std::runtime_error("Original negative-time title key lost");
    ++checks;
    for(unsigned message:{0x81u,0x82u,0x83u,0x84u,0x85u,0x86u,0x8bu,0x8cu,0x92u,0x8du,0x8eu}) {
      if(title.layoutJson(4000+message).find("\"glyphs\":[")==std::string::npos)throw std::runtime_error("Missing original help");
      ++checks;
    }
    if(title.layoutJson(5017).find("\"glyphs\":[")==std::string::npos)throw std::runtime_error("Missing original card Exit message");
    ++checks;
    for(unsigned id=0;id<3000;++id)for(unsigned frame=0;frame<128;++frame) {
      const auto *image=title.texture(id,frame);if(!image)break;
      if(!image->width||!image->height||image->rgba.size()!=std::size_t(image->width)*image->height*4)
        throw std::runtime_error("Invalid original texture "+std::to_string(id));
      ++checks;
    }
    if(!title.texture(263,9)||!title.texture(513,9)||!title.texture(8,7))throw std::runtime_error("Authored digits/title animation frames absent");
    ++checks;
    // CardID::init clears IdTex and registers only save_e.dat. Compare the
    // complete published CARD namespace with that pack, including ABSENT
    // frames: CORE also owns ID39, but its 15 images are not CARD frames.
    std::ifstream cardFile(std::filesystem::path(argv[1])/"ss/Cmn/save_e.dat",std::ios::binary);
    if(!cardFile)throw std::runtime_error("Original CARD archive missing");
    const std::vector<std::uint8_t> cardBytes((std::istreambuf_iterator<char>(cardFile)),{});
    const auto cardTextures=re4::DecodeSubscreenIdTextures(re4::CopySubscreenEntry(cardBytes,4));
    std::map<unsigned,unsigned> cardFrames,cardAnimationFrames;
    for(const auto &original:cardTextures) {
      const auto *published=title.texture(512+original.id,original.frame);
      if(!published || published->width!=original.image.width || published->height!=original.image.height ||
          published->rgba!=original.image.rgba)
        throw std::runtime_error("CARD image differs from its own archive");
      cardFrames[original.id]=std::max(cardFrames[original.id],original.frame+1);
      cardAnimationFrames[original.id]=original.animationFrames;
      ++checks;
    }
    for(unsigned id=0;id<256;++id) {
      if(title.texture(512+id,cardFrames[id]))
        throw std::runtime_error("Foreign CORE/SS frames leaked into CARD texture "+std::to_string(id));
      ++checks;
    }
    for(unsigned group:{2005u,2008u}) {
      const auto json=title.layoutJson(group);const auto at=json.find("\"texture\":551,");
      // Include the delimiter: searching ':1' also accepted ':15' in K10.
      const auto header=at==std::string::npos?std::string{}:json.substr(at,json.find("}",at)-at);
      if(header.find("\"textureFrames\":1,")==std::string::npos ||
          header.find("\"textureAnimationFrames\":1,")==std::string::npos)
        throw std::runtime_error("LOAD/SAVE static header incorrectly animates effect maps");
      ++checks;
    }
    for(unsigned group:{2005u,2006u,2007u,2008u,2009u}) {
      const auto json=title.layoutJson(group);
      std::size_t start=0;
      while((start=json.find("\"texture\":",start))!=std::string::npos) {
        const auto number=start+10;
        const auto texture=unsigned(std::stoul(json.substr(number)));
        const auto unit=json.substr(start,json.find('}',start)-start);
        if(texture!=255) {
          const auto id=texture-512;
          if(!cardFrames[id] || unit.find("\"textureFrames\":"+std::to_string(cardFrames[id])+",")==std::string::npos ||
              unit.find("\"textureAnimationFrames\":"+std::to_string(cardAnimationFrames[id])+",")==std::string::npos)
            throw std::runtime_error("CARD animation metadata differs from its own texture owner");
          ++checks;
        }
        start=number;
      }
    }
    if(argc==3){
      std::filesystem::create_directories(argv[2]);
      for(unsigned group:{3000u,5015u,5017u,5019u}){
        std::ofstream fixture(std::filesystem::path(argv[2])/(std::to_string(group)+".json"));
        fixture<<title.layoutJson(group);
        if(!fixture)throw std::runtime_error("Could not export original card fixture");
      }
    }
    std::cout << "M35 actual title/options/load/font assets PASS: " << checks << " checks\n";
  } catch(const std::exception &failure) {std::cerr << failure.what() << '\n';return 1;}
}
