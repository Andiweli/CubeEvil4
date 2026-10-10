// Must match GxMaterialState std140 offsets on all host/Android ABIs.
struct TevStage { ivec4 colorIn;ivec4 alphaIn;ivec4 colorOp;ivec4 alphaOp;ivec4 order;vec4 uv; };
layout(set=0,binding=8,std140) uniform MaterialState {
    TevStage stages[16];
    ivec4 konst[4];
    ivec4 meta;
    ivec4 alpha;
    ivec4 raster;
    ivec4 ambient;
    vec4 fog;
    vec4 fogColor;
    vec4 lights[40];
} gx;
