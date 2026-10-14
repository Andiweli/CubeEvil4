#include "display_brightness.h"
#include <stdexcept>
namespace re4 {
std::array<std::uint8_t,7> DisplayBrightnessFilter(int brightness) {
  if(brightness<MinimumDisplayBrightness || brightness>MaximumDisplayBrightness)
    throw std::runtime_error("Display: Brightness outside 34..114 bounds");
  constexpr std::array<unsigned,7> source{8,8,10,12,10,8,8};
  constexpr std::array<unsigned,7> order{5,1,3,0,4,2,6};
  const float rate=float(brightness)*(1.f/64.f);float remainder=float(brightness);
  std::array<std::uint8_t,7> filter{};
  for(unsigned i=0;i<7;++i) {filter[i]=std::uint8_t(float(source[i])*rate);remainder-=float(filter[i]);}
  for(unsigned priority=0;remainder>=1.f;++priority) {
    for(unsigned tap=0;tap<7;++tap)if(order[tap]==priority) {++filter[tap];break;}
    remainder-=1.f;
  }
  return filter;
}
} // namespace re4
