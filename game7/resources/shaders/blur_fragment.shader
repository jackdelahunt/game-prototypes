#version 460 core

in vec2 uv;

out vec4 frag_colour;

uniform sampler2D scene_texture;

void main()
{
    frag_colour = texture(scene_texture, uv);
}
