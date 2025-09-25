#version 460 core

layout (location = 0) in vec3 a_position;
layout (location = 1) in vec3 a_normal;
layout (location = 2) in vec2 a_uv;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

uniform mat4 sun_view;
uniform mat4 sun_projection;

out vec3 world_fragment_position;
out vec3 view_fragment_position;
out vec3 world_fragment_normal;
out vec3 view_fragment_normal;
out vec2 model_fragment_uv;
out vec4 sun_fragment_position;

void main()
{
    mat3 world_normal_matrix = transpose(inverse(mat3(model)));
    mat3 view_normal_matrix = transpose(inverse(mat3(view * model)));

    world_fragment_position = (model * vec4(a_position, 1)).xyz;
    view_fragment_position  = (view * model * vec4(a_position, 1)).xyz;
    world_fragment_normal   = normalize(world_normal_matrix * a_normal);
    view_fragment_normal    = normalize(view_normal_matrix * a_normal);
    model_fragment_uv       = a_uv;
    sun_fragment_position   = sun_projection * sun_view * vec4(world_fragment_position, 1);

    gl_Position = projection * view * model * vec4(a_position, 1);
}
