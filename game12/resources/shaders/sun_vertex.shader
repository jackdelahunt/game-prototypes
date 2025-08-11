#version 460 core

layout (location = 0) in vec3 a_position;
layout (location = 1) in vec3 a_normal;
layout (location = 2) in vec4 a_colour;
layout (location = 3) in vec2 a_uv;
layout (location = 4) in vec2 a_normal_uv;

uniform mat4 model;
uniform mat4 sun_space;

void main()
{
    gl_Position = sun_space * model * vec4(a_position, 1);
}
