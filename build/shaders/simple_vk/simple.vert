layout(location = 0) in vec4 position;
layout(location = 1) in vec2 texcoords;
layout(location = 2) in vec4 normal;

layout(push_constant) uniform uPushConstant
{
    mat4 view_projection;
    mat4 inv_view;
};

out gl_PerVertex
{
    vec4 gl_Position;
};

layout(location = 0) out struct
{
    vec4 position;
    vec2 texcoords;
    vec4 normal;
    vec4 cam_position;
} vertex_output;

void main()
{
    vertex_output.cam_position = inv_view[3];
    vertex_output.position = position;
    vertex_output.texcoords = texcoords;
    vertex_output.normal = normal;
    gl_Position = view_projection * position;
}