#version 460 core

in vec4 colour;
in vec2 uv;
flat in int draw_type;

out vec4 frag_colour;

uniform sampler2D atlas_texture;

void main()
{
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
        frag_colour = texture(atlas_texture, uv) * colour;
    }
} 
