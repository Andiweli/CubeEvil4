#include "startup_compositor.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
namespace re4 { namespace {
constexpr int Width=640,Height=480,CopyWidth=320,CopyHeight=224;
int integer(float v,int low,int high){if(!std::isfinite(v)||v<low||v>high||std::floor(v)!=v)throw std::invalid_argument("Title compositor integer");return int(v);}
int byte(float v){return std::clamp(int(v+.5f),0,255);}
std::uint32_t sample(const std::uint32_t *p,int width,int height,float x,float y){x=std::clamp(x,0.f,float(width-1));y=std::clamp(y,0.f,float(height-1));const int ix=int(x),iy=int(y),jx=std::min(width-1,ix+1),jy=std::min(height-1,iy+1);const float fx=x-ix,fy=y-iy;const auto a=p[iy*width+ix],b=p[iy*width+jx],c=p[jy*width+ix],d=p[jy*width+jx];std::uint32_t out=0;for(int shift=0;shift<=24;shift+=8){const float upper=((a>>shift)&255)*(1-fx)+((b>>shift)&255)*fx,lower=((c>>shift)&255)*(1-fx)+((d>>shift)&255)*fx;out|=std::uint32_t(byte(upper*(1-fy)+lower*fy))<<shift;}return out;}
int blendChannel(int s,int d,int alpha,int mode){switch(mode){case 1:return byte(s*(alpha/255.f)+d);case 2:return std::min(255,s+d);case 3:return byte(s*(d/255.f)+d);case 4:return byte(s*(d/255.f));default:return byte(s*(alpha/255.f)+d*(1-alpha/255.f));}}
std::uint32_t blend(std::uint32_t src,std::uint32_t dst,const float *tint,int alpha,int mode){if(alpha<=1)return dst;std::uint32_t out=0xff000000u;for(int k=0;k<3;++k){const int shift=16-k*8,s=byte(((src>>shift)&255)*tint[k]/255),d=(dst>>shift)&255;out|=std::uint32_t(blendChannel(s,d,alpha,mode))<<shift;}return out;}
std::uint32_t negative(std::uint32_t rgb,int pow){const int scale=1<<std::min(2,pow);std::uint32_t out=0xff000000u;for(int shift=0;shift<=16;shift+=8)out|=std::uint32_t(255-std::min(255,int((rgb>>shift)&255)*scale))<<shift;return out;}
float offset(int coefficient,int channel,bool signedWarp){const int product=coefficient*(channel-(signedWarp?128:0)); // GX arithmetic right shift is floor, also for negative values.
  const int fixed=product>=0?product/4:-((-product+3)/4);return fixed/128.f;
}
}
void ComposeStartupFrame(std::uint32_t *framebuffer,std::size_t frameCount,const std::uint32_t *texture,std::size_t textureCount,const std::uint32_t *mask,std::size_t maskCount,const float *p,std::size_t parameterCount){
  if(!framebuffer||frameCount!=std::size_t(Width*Height)||!texture||!p||(parameterCount!=25 && parameterCount!=29))throw std::invalid_argument("Title compositor buffer");
  for(std::size_t i=0;i<parameterCount;++i)if(!std::isfinite(p[i])||std::abs(p[i])>1.e6f)throw std::invalid_argument("Title compositor finite parameters");
  const int tw=integer(p[0],1,4096),th=integer(p[1],1,4096),mw=integer(p[2],0,4096),mh=integer(p[3],0,4096);
  if(textureCount!=std::size_t(tw)*th||(mw==0)!=(mh==0)||bool(mask)!=(mw>0&&mh>0)||(mask&&maskCount!=std::size_t(mw)*mh)||(!mask&&maskCount))throw std::invalid_argument("Title compositor texture dimensions");
  const int alpha=integer(p[17],0,255),type=integer(p[18],0,3),sub=integer(p[19],0,255),mode=integer(p[20],0,4),x0=integer(p[21],0,Width),y0=integer(p[22],0,Height),x1=integer(p[23],0,Width),y1=integer(p[24],0,Height);
  if(p[12]<=p[10]||p[13]<=p[11]||x1<x0||y1<y0)throw std::invalid_argument("Title compositor bounds");
  for(int i=14;i<=16;++i)if(p[i]<0||p[i]>255)throw std::invalid_argument("Title compositor tint");
  const float left=parameterCount==29?p[25]:0,top=parameterCount==29?p[26]:0,
      right=parameterCount==29?p[27]:Width,bottom=parameterCount==29?p[28]:Height;
  if(left<0 || top<0 || right>Width || bottom>Height || right-left<1 || bottom-top<1)
    throw std::invalid_argument("Title compositor sample rectangle");
  if(alpha<=1||x0==x1||y0==y1)return;
  thread_local std::array<std::uint32_t,CopyWidth*CopyHeight> copy;
  // Extend the active picture into the copy when the user-selected full
  // screen viewport discards letterbox IDs. Refraction then cannot pull
  // those discarded black bars back into the visible picture.
  if(type)for(int y=0;y<CopyHeight;++y)for(int x=0;x<CopyWidth;++x)copy[y*CopyWidth+x]=sample(framebuffer,Width,Height,
      std::clamp((x+.5f)*Width/CopyWidth-.5f,left,right-1),
      std::clamp((y+.5f)*Height/CopyHeight-.5f,top,bottom-1));
  // GXSetIndTexMtx's S0.10 truncation and exponent1. The decompiled depth
  // expression is uninitialized; retain the original minimum depth1500.
  const int coefficient=int(1024*(alpha/255.f)*.04f*1000/1500*(1+sub/32.f));
  const float invWidth=1/(p[12]-p[10]),invHeight=1/(p[13]-p[11]);
  for(int y=y0;y<y1;++y)for(int x=x0;x<x1;++x){const float sx=x+.5f,sy=y+.5f,lx=p[4]*sx+p[5]*sy+p[6],ly=p[7]*sx+p[8]*sy+p[9],u=(lx-p[10])*invWidth,v=(ly-p[11])*invHeight;if(u<0||u>1||v<0||v>1)continue;
    const auto texel=sample(texture,tw,th,u*tw-.5f,v*th-.5f);int a=alpha;std::uint32_t src;
    if(!type){src=texel;a=a*int(texel>>24)/255;}
    else if(type==1){src=negative(sample(copy.data(),CopyWidth,CopyHeight,sx*CopyWidth/Width-.5f,sy*CopyHeight/Height-.5f),sub);a=a*int(texel>>24)/255;}
    else{const float dx=offset(coefficient,int(texel>>24),type==3),dy=offset(coefficient,int(texel&255),type==3);src=sample(copy.data(),CopyWidth,CopyHeight,sx*CopyWidth/Width-.5f+dx,sy*CopyHeight/Height-.5f+dy);}
    if(mask)a=a*int(sample(mask,mw,mh,u*mw-.5f,v*mh-.5f)>>24)/255;
    const int index=y*Width+x;framebuffer[index]=blend(src,framebuffer[index],p+14,a,mode);
  }
}
}
