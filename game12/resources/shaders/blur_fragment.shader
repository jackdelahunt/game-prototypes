#version 460 core

layout(location = 0) out vec4 colour_attachment;

in vec2 uv;

uniform sampler2D ssao_map;

uniform int blur_radius;

void main() {
    vec2 texelSize = 1.0 / vec2(textureSize(ssao_map, 0));
    float result = 0.0;

    for (int x = -blur_radius; x < blur_radius; ++x) 
    {
        for (int y = -blur_radius; y < blur_radius; ++y) 
        {
            vec2 offset = vec2(float(x), float(y)) * texelSize;
            result += texture(ssao_map, uv + offset).r;
        }
    }

    int total_blur_samples = (blur_radius * 2) * (blur_radius * 2);
    colour_attachment = vec4(result / float(total_blur_samples));
}
