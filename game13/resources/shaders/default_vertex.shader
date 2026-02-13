#version 460 core

layout (location = 0) in vec4 a_position;
layout (location = 1) in vec4 a_colour;
layout (location = 2) in vec2 a_uv;
layout (location = 3) in vec2 a_normal_uv;
layout (location = 4) in int a_draw_type;

out vec4 colour;
out vec2 uv;
out int draw_type;

void main()
{
    colour = a_colour;
    uv = a_uv;
    draw_type = a_draw_type;

    gl_Position = a_position;
}
