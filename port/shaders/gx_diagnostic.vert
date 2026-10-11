#version 450
#extension GL_GOOGLE_include_directive : require
#include "gx_state.glsl"
layout(location=0) in vec3 position;
layout(location=1) in vec2 uv;
layout(location=2) in vec4 vertexColor;
layout(location=3) in vec3 normal;
layout(location=4) in vec3 rawNormal;
layout(location=0) out vec2 texCoord;
layout(location=1) out vec4 rasterColor;
layout(location=2) out float viewDepth;
layout(location=3) out vec3 generated[8];
layout(location=11) out vec4 sourceColor;
layout(location=12) out vec3 lightingColor;
layout(push_constant) uniform State { mat4 vp; vec4 viewRows[3]; vec4 reserved; } state;
void main() {
    sourceColor=vertexColor;
    vec4 clip=state.vp*vec4(position,1.0);
    // GX SDK perspective NDC z is [-1,0]. Vulkan is [0,1].
    gl_Position=vec4(clip.x,-clip.y,clip.z+clip.w,clip.w);
    texCoord=uv;
    for(int i=0;i<8;++i){
        generated[i]=vec3(0,0,1);
        if(i<gx.indirectMeta.y){
            TexGen g=gx.texgens[i];
            vec4 inputCoord=g.meta.y==1?vec4(rawNormal,1):vec4(uv,1,1);
            generated[i].x=dot(g.rows[0],inputCoord);
            generated[i].y=dot(g.rows[1],inputCoord);
            generated[i].z=g.meta.x==0?dot(g.rows[2],inputCoord):1;
        }
    }
    // Original LightSetModel chooses vertex ambient for BIN flag 0x40000000.
    vec3 viewPos=vec3(dot(state.viewRows[0],vec4(position,1)),dot(state.viewRows[1],vec4(position,1)),dot(state.viewRows[2],vec4(position,1)));
    vec3 viewNormal=vec3(dot(state.viewRows[0].xyz,normal),dot(state.viewRows[1].xyz,normal),dot(state.viewRows[2].xyz,normal));
    ivec3 lighting=gx.meta.y!=0?ivec3(round(vertexColor.rgb*255.0)):gx.ambient.rgb;
    int mask=int(gx.fog.w);
    for(int i=0;i<8;++i)if((mask&(1<<i))!=0){
        vec3 delta=gx.lights[i*5].xyz-viewPos;float distance=length(delta);
        vec3 direction=distance>0?delta/distance:vec3(0,0,1);
        float cosine=max(0.0,dot(direction,gx.lights[i*5+1].xyz));
        vec3 angle=vec3(1,cosine,cosine*cosine),dist=vec3(1,distance,distance*distance);
        float denominator=dot(gx.lights[i*5+3].xyz,dist);
        float attenuation=denominator!=0?max(0.0,dot(gx.lights[i*5+2].xyz,angle))/denominator:0;
        lighting+=ivec3(round(attenuation*max(0.0,dot(direction,viewNormal))*gx.lights[i*5+4].rgb));
    }
    ivec3 ambient=clamp(lighting,ivec3(0),ivec3(255));
    lightingColor=vec3(ambient)/255.0;
    ivec3 rgb=((ambient+(ambient>>7))*gx.raster.rgb)>>8;
    rasterColor=vec4(vec3(clamp(rgb,ivec3(0),ivec3(255)))/255.0,
        gx.meta.z!=0?vertexColor.a:float(gx.raster.a)/255.0);
    viewDepth=-viewPos.z;
}
