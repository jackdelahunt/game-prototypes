#version 460 core

layout(location = 0) out vec4 colour_attachment;

in vec2 uv;

uniform sampler2D scene_map;
uniform sampler2D ssao_map;

void main() {
    vec3 scene_sample = texture(scene_map, uv).rgb;
    float ssao_sample = texture(ssao_map, uv).r;

    colour_attachment = vec4(scene_sample * ssao_sample, 1);
}
