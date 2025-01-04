@header package shaders
@header import sg "../sokol/gfx"

@vs vs

in vec3 position;
in vec4 color0;
in vec2 texture_uv0;
in float texture_index0;

out vec4 color;
out vec2 texture_uv;
out float texture_index;

void main() {
    gl_Position = vec4(position, 1);
    color = color0;
    texture_uv = texture_uv0;
    texture_index = texture_index0;
}
@end

@fs fs
layout(binding=0) uniform texture2D default_texture;
layout(binding=1) uniform texture2D font_texture;
layout(binding=0) uniform sampler default_sampler;

in vec4 color;
in vec2 texture_uv;
in float texture_index;

out vec4 frag_color;

void main() {
    // solid
    if (texture_index == 0) {
        frag_color = color;
    }

    // circle
    if (texture_index == 1) {
        // normalise uvs to -0.5 -> 0.5, so the centre of the quad is 0,0
        vec2 normalised_uv = vec2(texture_uv.x - 0.5, texture_uv.y - 0.5);
        float d = distance(vec2(0, 0), normalised_uv);
        if (d <= 0.5) {
            frag_color = color;
        }
    }

    // texture
    if (texture_index == 2) {
        frag_color = texture(sampler2D(default_texture, default_sampler), texture_uv) * color;
    }

    // font
    if (texture_index == 3) {
        // font texture only has values in the red channel
        frag_color = texture(sampler2D(font_texture, default_sampler), texture_uv).r * color;
    }

    // visualise texture uvs
    if (false) {
        frag_color = vec4(texture_uv.x, texture_uv.y, 0, 0);
    }
}
@end

@program basic vs fs
