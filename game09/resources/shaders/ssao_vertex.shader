#version 460 core

layout (location = 0) in vec3 a_position;
layout (location = 1) in vec3 a_normal;
layout (location = 2) in vec4 a_colour;
layout (location = 3) in vec2 a_uv;
layout (location = 4) in vec2 a_normal_uv;

out vec2 uv;

void main()
{
    uv = a_uv;
    gl_Position = vec4(a_position, 1);
}
