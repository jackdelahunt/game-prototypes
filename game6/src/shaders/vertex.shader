#version 460 core

layout (location = 0) in vec3 a_position;
layout (location = 1) in vec4 a_colour;

out vec4 colour;

void main()
{
    colour = a_colour;
    gl_Position = vec4(a_position, 1.0);
}
