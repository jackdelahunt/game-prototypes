@header package shaders
@header import sg "../sokol/gfx"

@vs vs

in vec3 position;
in vec4 color0;
in vec2 texture_uv0;
out vec4 color;
out vec2 texture_uv;

void main() {
    gl_Position = vec4(position, 1);
    color = color0;
    texture_uv = texture_uv0;
}
@end

@fs fs
layout(binding=0) uniform texture2D default_texture;
layout(binding=0) uniform sampler default_sampler;

in vec4 color;
in vec2 texture_uv;
out vec4 frag_color;

void main() {
    // sample from texture, cant remove with if because then the uniforms get removed
    // in the generated code which is very annoying, so we do it and then overwrite it 
    // if we want
    frag_color = texture(sampler2D(default_texture, default_sampler), texture_uv) * color;

    // just colour
    if (false) {
        frag_color = color;
    }

    // visualise texture uvs
    if (false) {
        frag_color = vec4(texture_uv.x, texture_uv.y, 0, 0);
    }
}
@end

@program basic vs fs
