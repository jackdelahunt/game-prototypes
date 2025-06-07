#version 460 core

in vec4 colour;
in vec2 uv;
in vec2 normal_uv;
flat in int draw_type;

layout(location = 0) out vec4 g_position;
layout(location = 1) out vec4 g_normal;
layout(location = 2) out vec4 g_albedo;

uniform sampler2D atlas_texture;
uniform sampler2D font_texture;

void main()
{
    // rectangle
    if (draw_type == 0) {
        g_albedo = colour;
    }

    // circle
    if (draw_type == 1) {
        float d = length(uv - vec2(0.5));
        if (d > 0.5) {
            discard;
        }

        g_albedo = colour;
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

        g_albedo = final_colour;
    }

    // font
    if (draw_type == 3) {
        g_albedo = texture(font_texture, uv).r * colour;
    }
} 
