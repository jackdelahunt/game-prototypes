#version 460 core

in vec3 fragment_position;
in vec3 normal;
in vec4 colour;
in vec2 uv;

layout(location = 0) out vec4 g_position;
layout(location = 1) out vec4 g_normal;
layout(location = 2) out vec4 g_albedo;

uniform sampler2D atlas_texture;

void main()
{
    vec4 sample_colour = texture(atlas_texture, uv) * colour;

    // remove if 0 alpha so the empty pixels in the texture
    // dont add redundent info to the depth buffer and cover
    // things they shouldn't
    if(sample_colour.a == 0) {
        discard;
    }

    g_position = vec4(fragment_position, 1);
    g_normal = vec4(normalize(normal), 1);
    g_albedo = sample_colour;
}
