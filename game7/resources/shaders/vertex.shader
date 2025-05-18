#version 460 core

layout (location = 0) in vec3 a_position;
layout (location = 1) in vec4 a_colour;
layout (location = 2) in vec2 a_uv;
layout (location = 3) in int a_draw_type;

out vec4 colour;
out vec2 uv;
out int draw_type;

void main()
{
    gl_Position = vec4(a_position, 1.0);

    colour = a_colour;
    uv = a_uv;
    draw_type = a_draw_type;
}
