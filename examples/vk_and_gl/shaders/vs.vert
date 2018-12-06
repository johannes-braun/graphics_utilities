#version 460 core

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 uv;

layout(set = 0, binding = 0) uniform Camera
{
	mat4 view;
	mat4 proj;
	vec3 position;
} camera;

layout(location = 0) out vec3 out_normal;

void main()
{
	out_normal = normal;
	gl_Position = camera.proj * camera.view * vec4(position, 1);
}