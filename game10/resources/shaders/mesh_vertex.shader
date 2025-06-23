#version 460 core

layout (location = 0) in vec3 a_position;
layout (location = 1) in vec3 a_normal;
layout (location = 2) in vec2 a_texture_uv;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out vec3 normal;
out vec2 texture_uv;

void main()
{
    gl_Position = projection * view * model * vec4(a_position, 1);

    normal = a_normal;
    texture_uv = a_texture_uv;
}
