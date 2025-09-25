#version 460 core

layout (location = 0) in vec3 a_position;
layout (location = 1) in vec4 a_colour;
layout (location = 2) in vec2 a_uv;
layout (location = 3) in int a_draw_type;

out vec2 uv;

void main()
{
    uv = a_uv;
    gl_Position = vec4(a_position, 1);
}
