#version 450
#extension GL_GOOGLE_include_directive : require
#include "gx_state.glsl"
layout(location=0) in vec2 texCoord;
layout(location=1) in vec4 rasterColor;
layout(location=2) in float viewDepth;
layout(location=0) out vec4 color;
layout(set=0,binding=0) uniform sampler2D tex0;
layout(set=0,binding=1) uniform sampler2D tex1;
layout(set=0,binding=2) uniform sampler2D tex2;
layout(set=0,binding=3) uniform sampler2D tex3;
layout(set=0,binding=4) uniform sampler2D tex4;
layout(set=0,binding=5) uniform sampler2D tex5;
layout(set=0,binding=6) uniform sampler2D tex6;
layout(set=0,binding=7) uniform sampler2D tex7;
ivec4 registers[4];
ivec4 sampled,raster,constantColor;
vec4 sampleMap(int m,vec2 uv) {
    // Constant bindings do not require sampledImageArrayDynamicIndexing on ARM32.
    switch(m){case 0:return texture(tex0,uv);case 1:return texture(tex1,uv);
        case 2:return texture(tex2,uv);case 3:return texture(tex3,uv);
        case 4:return texture(tex4,uv);case 5:return texture(tex5,uv);
        case 6:return texture(tex6,uv);case 7:return texture(tex7,uv);}
    return vec4(0); // GX null map 255; unsupported maps are rejected by the CPU.
}
ivec3 colorInput(int id) {
    if(id<8)return (id&1)!=0?ivec3(registers[id/2].a):registers[id/2].rgb;
    switch(id){case 8:return sampled.rgb;case 9:return sampled.aaa;
        case 10:return raster.rgb;case 11:return raster.aaa;
        case 12:return ivec3(255);case 13:return ivec3(128);case 14:return constantColor.rgb;}
    return ivec3(0);
}
int alphaInput(int id) {
    if(id<4)return registers[id].a;
    switch(id){case 4:return sampled.a;case 5:return raster.a;case 6:return constantColor.a;}
    return 0;
}
int combine(int a,int b,int c,int d,ivec4 op) {
    a&=255;b&=255;c&=255;c+=c>>7;
    int shift=op.z==3?0:op.z;
    int fraction=((a<<8)+(b-a)*c)*(1<<shift);
    if(op.z!=3)fraction+=op.x==0?128:127;
    fraction>>=8;
    int value=(d+(op.y==1?128:op.y==2?-128:0))*(1<<shift);
    value+=op.x==0?fraction:-fraction;
    if(op.z==3)value>>=1;
    return clamp(value,(op.w&4)!=0?0:-1024,(op.w&4)!=0?255:1023);
}
bool compareAlpha(int v,int ref,int op) {
    switch(op){case 0:return false;case 1:return v<ref;case 2:return v==ref;
        case 3:return v<=ref;case 4:return v>ref;case 5:return v!=ref;case 6:return v>=ref;}
    return true;
}
void main() {
    for(int i=0;i<4;++i)registers[i]=ivec4(0);
    raster=ivec4(round(clamp(rasterColor,0.0,1.0)*255.0));
    for(int i=0;i<gx.meta.x;++i){
        TevStage s=gx.stages[i];
        sampled=ivec4(round(sampleMap(s.order.y,texCoord*s.uv.zw+s.uv.xy)*255.0));
        constantColor=ivec4(gx.konst[s.order.z].rgb,gx.konst[s.order.w].a);
        ivec3 a=colorInput(s.colorIn.x),b=colorInput(s.colorIn.y),c=colorInput(s.colorIn.z),d=colorInput(s.colorIn.w);
        int aa=alphaInput(s.alphaIn.x),ab=alphaInput(s.alphaIn.y),ac=alphaInput(s.alphaIn.z),ad=alphaInput(s.alphaIn.w);
        ivec3 rgb;for(int k=0;k<3;++k)rgb[k]=combine(a[k],b[k],c[k],d[k],s.colorOp);
        int alp=combine(aa,ab,ac,ad,s.alphaOp);
        registers[s.colorOp.w&3].rgb=rgb;registers[s.alphaOp.w&3].a=alp;
    }
    int alpha=registers[0].a&255;
    bool a=compareAlpha(alpha,gx.alpha.y,gx.alpha.x),b=compareAlpha(alpha,int(gx.fog.z),gx.alpha.w);
    bool pass=gx.alpha.z==0?(a&&b):gx.alpha.z==1?(a||b):gx.alpha.z==2?(a!=b):(a==b);
    if(!pass)discard;
    color=vec4(vec3(registers[0].rgb&255)/255.0,float(alpha)/255.0);
    if(gx.meta.w!=0){
        float fog=clamp((viewDepth-gx.fog.x)/(gx.fog.y-gx.fog.x),0.0,1.0);
        if(gx.meta.w==4)fog=1-exp2(-8*fog);
        if(gx.meta.w==5)fog=1-exp2(-8*fog*fog);
        if(gx.meta.w==6)fog=exp2(-8*(1-fog));
        if(gx.meta.w==7)fog=exp2(-8*(1-fog)*(1-fog));
        int amount=int(round(fog*256));ivec3 source=registers[0].rgb&255;
        ivec3 fogRgb=ivec3(round(gx.fogColor.rgb*255));
        color.rgb=vec3((source*(256-amount)+fogRgb*amount)>>8)/255.0;
    }
}
