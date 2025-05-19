#version 460 core

struct Light {
    vec2 position;
};

in vec2 uv;

out vec4 frag_colour;

uniform sampler2D scene_texture;

uniform int light_count;
uniform Light lights[20];

void main()
{
    float cuttoff = 0.3;
    float distance = length(uv - vec2(0.5));

    if (distance < cuttoff) {
        float fade = cuttoff / distance;        

        frag_colour = texture(scene_texture, uv) * vec4(fade, fade, fade, 1);
    } else {
        frag_colour = vec4(0, 0, 0, 1);
    } 
} 
