#version 460 core

in vec4 colour;
in vec2 uv;

out vec4 frag_colour;

void main()
{
    frag_colour = vec4(uv.x, uv.y, 0, 1);
} 
