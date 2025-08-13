#version 460 core

layout(location = 0) out vec4 g_position;
layout(location = 1) out vec4 g_normal;
layout(location = 2) out vec4 g_albedo;

in vec3 fragment_position;
in vec3 normal;

uniform vec4 colour;

void main() {
    g_position = vec4(fragment_position, 1);
    g_albedo = colour;
    g_normal = vec4(normal, 1);
} 
