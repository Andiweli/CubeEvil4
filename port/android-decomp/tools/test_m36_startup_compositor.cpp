#include "startup_compositor.h"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>
namespace {
unsigned checks;
void check(bool pass,const char *message){++checks;if(!pass)throw std::runtime_error(message);}
std::vector<float> parameters(int type,int alpha=255,int mode=0){return {1,1,0,0,1,0,0,0,1,0,0,0,640,480,255,255,255,float(alpha),float(type),0,float(mode),0,0,640,480};}
void compose(std::vector<std::uint32_t>&frame,const std::vector<std::uint32_t>&texture,const std::vector<std::uint32_t>&mask,const std::vector<float>&p){re4::ComposeStartupFrame(frame.data(),frame.size(),texture.data(),texture.size(),mask.empty()?nullptr:mask.data(),mask.size(),p.data(),p.size());}
template<class T> std::vector<T> read(const char *path){std::ifstream f(path,std::ios::binary|std::ios::ate);if(!f)throw std::runtime_error("Read fixture");auto n=f.tellg();if(n<0||n>64*1024*1024||n%sizeof(T))throw std::runtime_error("Fixture budget");std::vector<T>v(static_cast<std::size_t>(n)/sizeof(T));f.seekg(0);f.read(reinterpret_cast<char *>(v.data()),n);if(!f)throw std::runtime_error("Fixture truncation");return v;}
}
int main(int argc,char **argv){try{
  if(argc==7&&std::string(argv[1])=="--compose"){auto frame=read<std::uint32_t>(argv[2]),texture=read<std::uint32_t>(argv[3]);auto p=read<float>(argv[5]);std::vector<std::uint32_t>mask;if(std::string(argv[4])!="-")mask=read<std::uint32_t>(argv[4]);compose(frame,texture,mask,p);std::ofstream out(argv[6],std::ios::binary);out.write(reinterpret_cast<const char *>(frame.data()),std::streamsize(frame.size()*4));return out?0:2;}
  if(argc!=1)throw std::runtime_error("Usage: startup_compositor_check [--compose FRAME_ARGB TEXTURE_ARGB MASK_ARGB|- PARAMETERS_F32 OUTPUT_ARGB]");
  constexpr std::size_t Count=640*480;std::vector<std::uint32_t>frame(Count,0xff804020u),tex={0xffffffffu},mask;auto p=parameters(1);compose(frame,tex,mask,p);check(std::all_of(frame.begin(),frame.end(),[](auto rgb){return rgb==0xff7fbfdfu;}),"Original zero-power negative shader");
  frame.assign(Count,0xff804020u);p[19]=1;compose(frame,tex,mask,p);check(frame[140*640+220]==0xff007fbfu,"Negative scaling/clamp");
  frame.assign(Count,0xff404040u);tex[0]=0xff808080u;p=parameters(0,128,0);compose(frame,tex,mask,p);check(frame[0]==0xff606060u,"Source-alpha blend");
  const std::uint32_t expected[]={0xff606060u,0xff808080u,0xffc0c0c0u,0xff606060u,0xff202020u};for(int mode=0;mode<5;++mode){frame.assign(Count,0xff404040u);p=parameters(0,128,mode);compose(frame,tex,mask,p);check(frame[0]==expected[mode],"Original blend factors");}
  frame.assign(Count,0xff404040u);p=parameters(0,1,2);compose(frame,tex,mask,p);check(frame[0]==0xff404040u,"Original alpha-test threshold");
  tex[0]=0xffffffffu;frame.assign(Count,0xff101010u);p=parameters(1);p[2]=p[3]=1;mask={0x00ffffffu};compose(frame,tex,mask,p);check(frame[0]==0xff101010u,"Mask affects shader alpha");mask.clear();
  frame.assign(Count,0xff202020u);tex[0]=0xffffffffu;p=parameters(0);p[10]=10;p[11]=20;p[12]=30;p[13]=40;p[21]=10;p[22]=20;p[23]=30;p[24]=40;compose(frame,tex,mask,p);check(frame[25*640+15]==0xffffffffu&&frame[25*640+9]==0xff202020u&&frame[40*640+15]==0xff202020u,"Authored quad coverage");
  std::vector<std::uint32_t>gradient(Count);for(int y=0;y<480;++y)for(int x=0;x<640;++x)gradient[y*640+x]=0xff000000u|(std::uint32_t(x/3)<<16)|std::uint32_t(y/2);
  frame=gradient;tex[0]=0xff000000u;p=parameters(2);compose(frame,tex,mask,p);check(((frame[240*640+150]>>16)&255)>((gradient[240*640+150]>>16)&255),"GX indirect horizontal channel must be A");check((frame[240*640+150]&255)==(gradient[240*640+150]&255),"GX indirect vertical channel must be B");
  frame=gradient;tex[0]=0x007f007fu;p=parameters(3);compose(frame,tex,mask,p);check(((frame[240*640+150]>>16)&255)<((gradient[240*640+150]>>16)&255),"Signed GX indirect bias");
  // Stretching the active 360-line picture must not refract letterbox black
  // back into its bottom edge (the supplied TITLE6 type2 transition).
  std::vector<std::uint32_t> letterbox(Count,0xff000000u);
  for(unsigned y=60;y<420;++y)std::fill(letterbox.begin()+y*640,letterbox.begin()+(y+1)*640,0xff609040u);
  tex={0xff0000ffu};p=parameters(2);p[11]=60;p[13]=420;p[22]=60;p[24]=420;
  auto oldLetterbox=letterbox;compose(oldLetterbox,tex,mask,p);
  check(oldLetterbox[415*640+320]!=0xff609040u,"Fixture actually pulls letterbox into active content");
  p.insert(p.end(),{0,60,640,420});frame=letterbox;compose(frame,tex,mask,p);
  check(frame[415*640+320]==0xff609040u && frame[65*640+320]==0xff609040u,"Active EFB copy extends picture edges through refraction");
  check(frame[450*640+320]==0xff000000u,"Copy rectangle does not draw outside the authored effect");
  auto invalid=[&](std::vector<float>q){bool rejected=false;auto unchanged=gradient;try{compose(unchanged,tex,mask,q);}catch(const std::invalid_argument&){rejected=true;}check(rejected&&unchanged==gradient,"Invalid parameters accepted/changed framebuffer");};p=parameters(1);auto q=p;q[0]=4097;invalid(q);q=p;q[1]=2;invalid(q);q=p;q[17]=-1;invalid(q);q=p;q[18]=4;invalid(q);q=p;q[20]=5;invalid(q);q=p;q[21]=-1;invalid(q);q=p;q[22]=std::numeric_limits<float>::quiet_NaN();invalid(q);q=p;q[23]=641;invalid(q);q=p;q[24]=-1;invalid(q);q=p;q[12]=q[10];invalid(q);q=p;q[4]=std::numeric_limits<float>::infinity();invalid(q);q=p;q[14]=256;invalid(q);q=p;q.pop_back();invalid(q);
  tex.assign(64*64,0xff7f7f7fu);p=parameters(2,160);p[0]=p[1]=64;p[11]=60;p[13]=420;p[22]=60;p[24]=420;auto start=std::chrono::steady_clock::now();for(int n=0;n<30;++n){frame=gradient;compose(frame,tex,mask,p);}double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();std::cout<<"M36 startup EFB/indirect/blend/validation PASS: "<<checks<<" checks; 30 host shimmer frames "<<ms<<" ms ("<<ms/30<<" ms/frame, not a device measurement)\n";
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
