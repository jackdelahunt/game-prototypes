#version 460 core

struct Light {
    vec2 position;
    vec4 colour;
};

in vec2 uv;

out vec4 frag_colour;

uniform vec4 global_light;
uniform sampler2D scene_texture;
uniform int light_count;
uniform Light lights[20];

void main()
{
    vec2 position = lights[0].position;
    vec4 colour = lights[0].colour;

    frag_colour = texture(scene_texture, uv) * global_light;

    float cuttoff = 0.3;
    vec2 ndc_uv = (uv * 2) - 1;
    float distance = length(ndc_uv - position);

    if (distance < cuttoff) {
        float fade = distance / cuttoff;
        vec4 final_colour = mix(colour, global_light, fade * fade);

        frag_colour = texture(scene_texture, uv) * final_colour;
    }

    for(int i = 0; i < light_count; i++) {

    }
}
