#version 460 core

struct Light {
    vec2 position;
    float radius;
};

in vec2 uv;

out vec4 frag_colour;

uniform vec4 global_light;
uniform vec4 light_colour;

uniform sampler2D scene_texture;

uniform int light_count;
uniform Light lights[20];

uniform float aspect_ratio; // width / height

void main()
{
    // convert uv coord to ndc and then scale based on aspect ratio
    vec2 uv_to_ndc = (uv * 2) - 1;
    vec2 scaled_ndc = vec2(uv_to_ndc.x * aspect_ratio, uv_to_ndc.y);

    vec4 base_colour = texture(scene_texture, uv);
    float total_light_mix_amount = 0;

    for(int i = 0; i < light_count; i++) {
        Light light = lights[i];

        float distance = length(scaled_ndc - light.position);
        if (distance < light.radius) {
            total_light_mix_amount += 1 - (distance / light.radius);
        }
    }

    frag_colour = base_colour * mix(global_light, light_colour, total_light_mix_amount);
    // frag_colour = vec4(total_light_mix_amount);
}
