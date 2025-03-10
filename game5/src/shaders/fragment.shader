#version 460 core

in vec4 colour;
in vec4 highlight_colour;
in vec2 uv;
flat in int draw_type;

out vec4 frag_colour;

uniform sampler2D face_texture;
uniform sampler2D font_texture;

void main()
{
    // rectangle 
    if (draw_type == 0) {
        frag_colour = colour;
    }

    // circle
    if (draw_type == 1) {
        // normalise uvs to -0.5 -> 0.5, so the centre of the quad is 0,0
        vec2 normalised_uv = vec2(uv.x - 0.5, uv.y - 0.5);
        float d = distance(vec2(0, 0), normalised_uv);

        if (d > 0.5) {
            discard;
        }

        frag_colour = colour;
    }

    // texture
    if (draw_type == 2) {
        vec4 texture_colour = texture(face_texture, uv);
        if (texture_colour == vec4(1, 0, 1, 1)) {
            texture_colour = highlight_colour;
        }

        frag_colour = texture_colour * colour;
    }

    // font
    if (draw_type == 3) {
        frag_colour = texture(font_texture, uv).r * colour;
    }


    // frag_colour = vec4(uv.x, uv.y, 0, 1); // texture uvs
} 
