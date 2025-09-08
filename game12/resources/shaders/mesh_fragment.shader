#version 460 core

layout(location = 0) out vec4 g_position;
layout(location = 1) out vec4 g_normal;
layout(location = 2) out vec4 g_albedo;

in vec3 fragment_position;
in vec3 normal;
in vec2 uv;

uniform vec4 colour;

uniform sampler2D material_albedo;
uniform sampler2D material_ambient_occlusion;
uniform vec2 tiling_factor;

void main() {
    vec4 albedo_sample = texture(material_albedo, uv * tiling_factor);
    if (albedo_sample.a == 0) {
        discard;
    }

    vec4 ambient_occlusion_sample = texture(material_ambient_occlusion, uv * tiling_factor);

    g_position = vec4(fragment_position, 1);
    g_albedo = (albedo_sample * colour) * ambient_occlusion_sample;
    g_normal = vec4(normal, 1);
} 
