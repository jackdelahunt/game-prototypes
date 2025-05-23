#version 460 core

struct Light {
    vec2 position;
};

in vec2 uv;

out vec4 frag_colour;

uniform vec4 global_light;
uniform vec4 light_colour;
uniform sampler2D scene_texture;
uniform int light_count;
uniform Light lights[20];

void main()
{
    float cutoff = 0.4;
    vec2 ndc_uv = (uv * 2) - 1;

    vec4 base_colour = texture(scene_texture, uv);
    float total_light_mix_amount = 0;

    for(int i = 0; i < light_count; i++) {
        Light light = lights[i];

        float distance = length(ndc_uv - light.position);
        if (distance < cutoff) {
            total_light_mix_amount += 1 - (distance / cutoff);
        }
    }

    frag_colour = base_colour * mix(global_light, light_colour, total_light_mix_amount);
    // frag_colour = vec4(total_light_mix_amount);
}
