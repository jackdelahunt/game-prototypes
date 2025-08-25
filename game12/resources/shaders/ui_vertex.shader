#version 460 core

layout (location = 0) in vec3 a_position;
layout (location = 1) in vec4 a_colour;
layout (location = 2) in vec2 a_uv;
layout (location = 3) in int a_draw_type;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out vec3 position;
out vec4 colour;
out vec2 uv;
out int draw_type;

void main()
{
    position    = a_position;
    colour      = a_colour;
    uv          = a_uv;
    draw_type   = a_draw_type;

    gl_Position = projection * view * model * vec4(a_position, 1);
}
