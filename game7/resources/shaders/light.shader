#version 460 core

struct Light {
    vec2 position;
    vec4 colour;
};

in vec2 uv;

out vec4 frag_colour;

uniform sampler2D scene_texture;
uniform Light light;

void main()
{
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
