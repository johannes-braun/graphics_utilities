layout(points) in;
layout(line_strip, max_vertices = 4) out;

layout(location=0) in vec2 in_pixel[];


layout(binding = 0) uniform Data
{
    mat4 vp;
    sampler2D tex;
};


in gl_PerVertex
{
    vec4 gl_Position;
float gl_PointSize;
float gl_ClipDistance[];
} gl_in[];

out gl_PerVertex
{
    vec4 gl_Position;
};

layout(location=0) out vec2 pixel;

const vec4 luma = vec4(0.299, 0.587, 0.114, 1);
void main()
{
    vec4 color_00 = texelFetch(tex, ivec2(in_pixel[0]), 0);
    vec4 color_01 = texelFetch(tex, ivec2(in_pixel[0]) + ivec2(0, 1), 0);
    vec4 color_10 = texelFetch(tex, ivec2(in_pixel[0]) + ivec2(1, 0), 0);
    vec4 color_11 = texelFetch(tex, ivec2(in_pixel[0]) + ivec2(1, 1), 0);

    float val_00 = (dot(luma, color_00)-0.5f);
    float val_01 = (dot(luma, color_01)-0.5f);
    float val_10 = (dot(luma, color_10)-0.5f);
    float val_11 = (dot(luma, color_11)-0.5f);

    pixel = in_pixel[0];

    int count = 0;
   // if (val_00 * val_10 < 0)
    {
        vec3 p1 = gl_in[0].gl_Position.xyz + vec3(0, 0, 0);
        vec3 p2 = gl_in[0].gl_Position.xyz + vec3(1, 0, 0);
        gl_Position.a = 1;
        gl_Position.xyz = mix(p1, p2, val_00/(val_10 - val_00));
        gl_Position = vp * gl_Position;
        EmitVertex();
        ++count;
    }

    //if (val_00 * val_01 < 0)
    {
        vec3 p1 = gl_in[0].gl_Position.xyz + vec3(0, 0, 0);
        vec3 p2 = gl_in[0].gl_Position.xyz + vec3(0, 0, 1);
        gl_Position.a = 1;
        gl_Position.xyz = mix(p1, p2, val_01/(val_01 - val_00));
        gl_Position = vp * gl_Position;
        EmitVertex();
        ++count;
        if (count == 2)
        {
            count = 0;
            EndPrimitive();
        }
    }

    //if (val_10 * val_11 < 0)
    {
        vec3 p1 = gl_in[0].gl_Position.xyz + vec3(1, 0, 0);
        vec3 p2 = gl_in[0].gl_Position.xyz + vec3(1, 0, 1);
        gl_Position.a = 1;
        gl_Position.xyz = mix(p1, p2, val_11/(val_11 - val_10));
        gl_Position = vp * gl_Position;
        EmitVertex();
        ++count;
        if (count == 2)
        {
            count = 0;
            EndPrimitive();
        }
    }

    //if (val_01 * val_11 < 0)
    {
        vec3 p1 = gl_in[0].gl_Position.xyz + vec3(0, 0, 1);
        vec3 p2 = gl_in[0].gl_Position.xyz + vec3(1, 0, 1);
        gl_Position.a = 1;
        gl_Position.xyz = mix(p1, p2, val_11/(val_11 - val_01));
        gl_Position = vp * gl_Position;
        EmitVertex();
        ++count;
        if (count == 2)
        {
            count = 0;
            EndPrimitive();
        }
    }
}