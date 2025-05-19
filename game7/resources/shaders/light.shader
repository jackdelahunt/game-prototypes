#version 460 core

struct Light {
    vec2 position;
    vec4 colour;
};

in vec2 uv;

out vec4 frag_colour;

uniform sampler2D scene_texture;
uniform Light light;

vec4 global_light = vec4(0.2, 0.2, 0.5, 1);

void main()
{
    frag_colour = texture(scene_texture, uv) * global_light;

    float cuttoff = 0.3;
    vec2 ndc_uv = (uv * 2) - 1;
    float distance = length(ndc_uv - light.position);

    if (distance < cuttoff) {
        float fade = distance / cuttoff;
        vec4 final_colour = mix(light.colour, global_light, fade * fade);

        frag_colour = texture(scene_texture, uv) * final_colour;
    } 
} 

void lights() {
    float cuttoff = 0.4;
    vec2 ndc_uv = (uv * 2) - 1;
    float distance = length(ndc_uv - light.position);

    if (distance < cuttoff) {
        float fade = distance / cuttoff;
        vec4 final_colour = mix(light.colour, vec4(0, 0, 0, 1), fade);

        frag_colour = texture(scene_texture, uv) * final_colour;
    } else {
        frag_colour = vec4(0, 0, 0, 1);
    }
}
