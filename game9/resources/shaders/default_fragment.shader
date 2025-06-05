#version 460 core

in vec4 colour;
in vec2 uv;
in vec2 normal_uv;
flat in int draw_type;

layout(location = 0) out vec4 frag_colour;
layout(location = 1) out vec4 normal_colour;

uniform sampler2D atlas_texture;
uniform sampler2D font_texture;

void main()
{
    normal_colour = vec4(0, 0, 1, 1);

    // rectangle
    if (draw_type == 0) {
        frag_colour = colour;
    }

    // circle
    if (draw_type == 1) {
        float d = length(uv - vec2(0.5));
        if (d > 0.5) {
            discard;
        }

        frag_colour = colour;
    }

    // texture
    if (draw_type == 2) {
        vec4 final_colour = texture(atlas_texture, uv) * colour;

        // remove if 0 alpha so the empty pixels in the texture
        // dont add redundent info to the depth buffer and cover
        // things they shouldn't
        if(final_colour.a == 0) {
            discard;
        }

        frag_colour = final_colour;
        normal_colour = texture(atlas_texture, normal_uv);
    }

    // font
    if (draw_type == 3) {
        frag_colour = texture(font_texture, uv).r * colour;
    }
} 
