#version 460 core

#include "../api.glsl"

layout(location = 0) in vec4 in_color;
layout(location = 1) in vec2 in_uv;

layout(location = 0) out vec4 color;

layout(loc_gl(0) loc_vk(0, 0)) uniform sampler2D img;

void main()
{
	color = /*vec4(in_color.rgb, 1) */ texture(img, vec2(in_uv.x, 1-in_uv.y));

	if(color.a < 0.3) discard;
}