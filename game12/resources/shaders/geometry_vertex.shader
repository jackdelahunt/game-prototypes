#version 460 core

layout (location = 0) in vec3 a_position;
layout (location = 1) in vec3 a_normal;
layout (location = 2) in vec4 a_colour;
layout (location = 3) in vec2 a_uv;
layout (location = 4) in vec2 a_normal_uv;

out vec3 fragment_position;
out vec4 fragment_sun_position;
out vec3 normal;
out vec3 view_normal;
out vec4 colour;
out vec2 uv;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat4 sun_space;

void main()
{
    vec4 view_position = view * model * vec4(a_position, 1);

    gl_Position = projection * view_position;
    fragment_position = view_position.xyz;
    fragment_sun_position = sun_space * model * vec4(a_position, 1);

    mat3 normal_matrix = transpose(inverse(mat3(view * model)));
    view_normal = normal_matrix * a_normal;

    normal = a_normal;
    colour = a_colour;
    uv = a_uv;
}
