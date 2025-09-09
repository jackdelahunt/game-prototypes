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
    fragment_position = (model * vec4(a_position, 1)).xyz;

    mat3 normal_matrix = transpose(inverse(mat3(model)));
    normal = normalize(normal_matrix * a_normal);

    uv = a_uv;

    gl_Position = projection * view * model * vec4(a_position, 1);
}
