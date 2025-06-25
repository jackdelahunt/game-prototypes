#version 460 core

in vec4 colour;
in vec2 uv;
in vec2 normal_uv;
flat in int draw_type;

layout(location = 0) out vec4 frag_colour;

uniform sampler2D scene_texture;

float brightness(vec4 colour) {
    return (colour.r + colour.g + colour.b) * 0.33;
}

vec4 change_brightness(vec4 colour, float brightness) {
    return vec4(normalize(colour.rgb) * brightness, 1);
}

vec4 cell_shade(vec4 colour, int levels) {
#if 0
    // Convert color to grayscale luminance (or use brightness)
    float brightness = dot(colour.rgb, vec3(0.299, 0.587, 0.114));

    // Quantize brightness
    float quantized = floor(brightness * levels) / float(levels);

    // Scale original color to match quantized brightness
    return colour * (quantized / brightness);
#endif

    vec3 quantizedColor = floor(colour.rgb * float(levels)) / float(levels);
    return vec4(quantizedColor, 1.0);
}

void main()
{
    vec4 sample_colour = texture(scene_texture, uv);

    frag_colour = sample_colour;

    // frag_colour = cell_shade(frag_colour, 5);

    // game correction - same as sRGB but it is only applied on the final fragment colour
    float gamma = 2.2;
    frag_colour.rgb = pow(frag_colour.rgb, vec3(1.0 / gamma));
} 
