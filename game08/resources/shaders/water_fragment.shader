#version 460 core

in vec2 uv;

out vec4 frag_colour;

uniform sampler2D scene_texture;
uniform sampler2D water_texture;

uniform float water_level;

void main()
{
#if 0
    if(uv.y >= water_level - 0.002 && uv.y <= water_level + 0.002) {
        frag_colour = vec4(1);
        return;
    }
#endif

    if (texture(water_texture, uv) != vec4(1)) { 
        frag_colour = texture(scene_texture, uv);
        return;
    }

    float distance_to_surface = water_level - uv.y;
    float reflected_y = distance_to_surface + water_level;

    vec4 reflect_colour = texture(scene_texture, vec2(uv.x, reflected_y));
    float reflect_amount = min(0.4, 1 - distance_to_surface * 4);

    reflect_amount = clamp(reflect_amount, 0, 1);

    frag_colour = mix(texture(scene_texture, uv), reflect_colour, reflect_amount);
}
