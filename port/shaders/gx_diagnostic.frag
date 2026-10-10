#version 450
layout(location=0) in vec2 texCoord;
layout(location=0) out vec4 color;
layout(set=0,binding=0) uniform sampler2D material;
layout(set=0,binding=1) uniform sampler2D alphaTexture;
layout(push_constant) uniform State { mat4 vp; vec4 alpha; vec4 tint; } state;
void main() {
    color=texture(material,texCoord);
    // materialSetup alphaIn=GX_CA_RASA, not the diffuse texture's alpha.
    // Diagnostic model channel is the original initial opaque material alpha.
    color.rgb*=state.tint.rgb;
    color.a=state.tint.a;
    if(state.alpha.y!=0.0) {
        color.a*=texture(alphaTexture,texCoord).a;
        // alphaSetup GX_GREATER ref AND GX_GREATER 255 (second operand false)
        // uses GX_OR (1): effective condition is alpha > ref.
        if(color.a<=state.alpha.x) discard;
    }
}
