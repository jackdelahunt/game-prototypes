#version 460 core

layout (location = 0) in vec3 a_position;
layout (location = 1) in vec3 a_normal;
layout (location = 2) in vec4 a_colour;
layout (location = 3) in vec2 a_uv;
layout (location = 4) in vec2 a_normal_uv;

out vec3 fragment_position;
out vec4 fragment_sun_position;
out vec3 normal;
out vec4 colour;
out vec2 uv;
out vec2 normal_uv;

uniform sampler2D atlas_texture;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat4 sun_space;

void main()
{
    gl_Position = projection * view * model * vec4(a_position, 1);
    fragment_position = (model * vec4(a_position, 1)).xyz;
    fragment_sun_position = sun_space * model * vec4(a_position, 1);

    normal = a_normal;
    colour = a_colour;
    uv = a_uv;
    normal_uv = a_normal_uv;
}
