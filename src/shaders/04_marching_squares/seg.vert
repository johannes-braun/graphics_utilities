#version 460 core

#include "../api.glsl"

layout(loc_gl(0) loc_vk(0, 1)) uniform Data
{
    mat4 vp;
};

layout(loc_gl(0) loc_vk(0, 0)) uniform sampler2D tex;

out gl_PerVertex
{
    vec4 gl_Position;
};

layout(location=0) out vec2 pixel;

void main()
{
    ivec2 sz = textureSize(tex, 0)-1;
    float px = (gl_VertexID % sz.x);
    float pz = (gl_VertexID / sz.x);

    gl_Position = vec4(px, 0, pz, 1);
    pixel = vec2(px, pz);
}