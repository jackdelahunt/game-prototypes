#version 460 core

layout(location = 0) out vec4 g_position;
layout(location = 1) out vec4 g_normal;
layout(location = 2) out vec4 g_albedo;

in vec3 position;
in vec4 colour;
in vec2 uv;
flat in int draw_type;

uniform sampler2D atlas_texture;
uniform sampler2D font_texture;

void main()
{
    // rectangle
    if (draw_type == 0) {
        g_position = colour;
    }

    // circle
    if (draw_type == 1) {
        float d = length(uv - vec2(0.5));
        if (d > 0.5) {
            discard;
        }

        g_position = colour;
    }

    // texture
    if (draw_type == 2) {
        vec4 sample_colour = texture(atlas_texture, uv);

        // remove if 0 alpha so the empty pixels in the texture
        // dont add redundent info to the depth buffer and cover
        // things they shouldn't
        if(sample_colour.a == 0) {
            discard;
        }

        g_position = sample_colour * colour;
    }

    // font
    if (draw_type == 3) {
        vec4 sample_colour = texture(font_texture, uv);
        // if (sample_colour.r <= 0.01) {
            // discard;
        // }

        g_position = sample_colour.r * colour;
    }
} 
