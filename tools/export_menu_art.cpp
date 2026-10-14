#include "subscreen_data.h"
#include <filesystem>
#include <fstream>
#include <iostream>

// QA-only: decode supplied Disc files into a temporary output directory.
// No original image/model bytes are included in the source distribution.
int main(int argc,char **argv) {
  if(argc!=3) { std::cerr<<"usage: export_menu_art Disc1/files output-directory\n";return 2; }
  try {
    const std::string files=argv[1],directory=argv[2];
    std::filesystem::create_directories(directory);
    for(unsigned kind=0;kind<5;++kind) {
      const auto art=re4::LoadSubscreenArt(files,static_cast<re4::SubscreenArtKind>(kind));
      std::ofstream(directory+"/"+std::to_string(kind)+".json")<<art.layoutJson();
      for(const auto &entry:art.textures) {
        std::ofstream image(directory+"/"+std::to_string(kind)+"_"+std::to_string(entry.id)+"_"+std::to_string(entry.frame)+".rgba",std::ios::binary);
        image.write(reinterpret_cast<const char *>(entry.image.rgba.data()),entry.image.rgba.size());
      }
    }
    for(const auto &entry:std::vector<std::pair<std::string,re4::Texture>>{
        {"case10",re4::LoadOriginalCaseBoardImage(files,10,6)},
        {"portrait33",re4::LoadOriginalMenuPortraitImage(files,0x21)},
        {"icon33",re4::LoadOriginalItemIcon(files,0x21)}}) {
      const auto &image=entry.second;
      std::ofstream data(directory+"/"+entry.first+".rgba",std::ios::binary);
      data.write(reinterpret_cast<const char *>(image.rgba.data()),image.rgba.size());
      std::ofstream(directory+"/"+entry.first+".json")<<"{\"width\":"<<image.width<<",\"height\":"<<image.height<<"}";
    }
    std::cout<<"Original common/case/item/map/file artwork exported for host pixel QA.\n";
  } catch(const std::exception &error) { std::cerr<<error.what()<<'\n';return 1; }
}
