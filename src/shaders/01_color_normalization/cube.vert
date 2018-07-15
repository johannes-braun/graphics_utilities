#version 460 core

layout(location = 0) in vec3 position;
layout(location = 1) in vec2 uv;

out gl_PerVertex
{
    vec4 gl_Position;
};

layout(binding = 0) uniform render_info
{
    mat4 view_projection;
    mat4 hat_mat;
    vec3 average;
};
const float scale = 4.f;

layout(location = 0) out vec2 out_uv;

void main()
{
    gl_Position = view_projection * (vec4(vec3(scale * 0.5f), 1.f) * (vec4(position*vec3(1.005f), 1.f) + vec4(1,1,1, 0.f)));
    out_uv = uv; 
}