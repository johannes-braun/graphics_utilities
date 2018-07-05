//! #version 450 core

layout(location=0) in vec2 uv;

layout(binding = 0) uniform sampler2D tex;

layout(location = 0) out vec4 color;

void main()
{
    color = texture(tex, uv);
    color.a = (1-color.r) * 0.9f;
    color.rgb = vec3(0.85f);
}