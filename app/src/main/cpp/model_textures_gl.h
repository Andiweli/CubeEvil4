#pragma once
#include "room_lighting_gl.h"
#include "texture.h"
#include "graphics_options_gl.h"
#include <cstring>
#include <optional>
#include <tuple>
namespace re4 {
// Rebind only when source sampler settings change; the caller subsequently
// binds its material textures to their usual texture units.
struct ModelTextureLodGl {
  std::vector<std::pair<unsigned,unsigned>> ranges;
  std::optional<std::tuple<bool,unsigned,unsigned,unsigned,bool>> bound;
  bool anisotropyChecked{},anisotropySupported{};
  float maximumAnisotropy=1;
  std::vector<unsigned> generatedLevels;
  std::vector<bool> generated;
  void append(const Texture &image) {
    ranges.emplace_back(image.minLOD,unsigned(image.mips.size()));
    unsigned size=std::max(image.width,image.height),level=0;while(size>1){size>>=1;++level;}
    generatedLevels.push_back(level);generated.push_back(false);
  }
  void bind(const std::vector<GLuint> &textures) {
    const auto *cut=CurrentRoomLightCut();
    const auto key=std::make_tuple(cut!=nullptr,cut?cut->minimumLod:0,cut?cut->maximumLod:0,unsigned(userAf),userMipmapping);
    if(bound && *bound==key)return;
    if(!anisotropyChecked) {
      const auto *extensions=reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS));
      anisotropySupported=extensions && (std::strstr(extensions,"GL_EXT_texture_filter_anisotropic") ||
          std::strstr(extensions,"GL_ARB_texture_filter_anisotropic"));
      if(anisotropySupported)glGetFloatv(0x84ff,&maximumAnisotropy);
      anisotropyChecked=true;
    }
    glActiveTexture(GL_TEXTURE0);
    for(unsigned i=0;i<textures.size();++i) {
      const auto [storedMin,last]=ranges.at(i);
      glBindTexture(GL_TEXTURE_2D,textures[i]);
      unsigned activeLast=userMipmapping?last:0;
      if(!last && userMipmapping && i<generatedLevels.size()) {
        activeLast=generatedLevels[i];
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAX_LEVEL,GLint(activeLast));
        if(!generated[i]){glGenerateMipmap(GL_TEXTURE_2D);generated[i]=true;}
      }else glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAX_LEVEL,GLint(activeLast));
      glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,activeLast?GL_LINEAR_MIPMAP_LINEAR:GL_LINEAR);
      glTexParameterf(GL_TEXTURE_2D,GL_TEXTURE_MIN_LOD,float(std::min(activeLast,cut?cut->minimumLod:storedMin)));
      glTexParameterf(GL_TEXTURE_2D,GL_TEXTURE_MAX_LOD,float(std::min(activeLast,cut?cut->maximumLod:activeLast)));
      if(anisotropySupported)glTexParameterf(GL_TEXTURE_2D,0x84fe,
          std::min(maximumAnisotropy,float(userAf)));
    }
    bound=key;
  }
};
}
