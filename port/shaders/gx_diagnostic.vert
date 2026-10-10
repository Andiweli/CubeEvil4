#version 450
layout(location=0) in vec3 position;
layout(location=1) in vec2 uv;
layout(location=0) out vec2 texCoord;
layout(push_constant) uniform State { mat4 vp; vec4 alpha; vec4 tint; } state;
void main() {
    vec4 clip=state.vp*vec4(position,1.0);
    // GX SDK perspective NDC z is [-1,0]. Vulkan is [0,1].
    gl_Position=vec4(clip.x,-clip.y,clip.z+clip.w,clip.w);
    texCoord=uv;
}
