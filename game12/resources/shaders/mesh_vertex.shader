#version 460 core

layout (location = 0) in vec3 a_position;
layout (location = 1) in vec3 a_normal;
layout (location = 2) in vec2 a_uv;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out vec3 fragment_position;
out vec3 normal;
out vec2 uv;

void main()
{
    vec4 view_position = view * model * vec4(a_position, 1);

    fragment_position = view_position.xyz;
    normal = a_normal;
    uv = a_uv;

    gl_Position = projection * view_position;
}
