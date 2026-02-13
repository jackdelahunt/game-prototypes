#version 460 core

layout (location = 0) in vec3 a_position;
layout (location = 1) in vec3 a_normal;
layout (location = 2) in vec4 a_colour;

out vec3 normal;
out vec4 colour;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main() {
    gl_Position = projection * view * model * vec4(a_position, 1);
    normal = a_normal;
    colour = a_colour;
}
