#version 460 core

in vec4 colour;
in vec2 uv;

out vec4 frag_colour;

uniform sampler2D atlas_texture;

void main()
{
    frag_colour = texture(atlas_texture, uv);
} 
