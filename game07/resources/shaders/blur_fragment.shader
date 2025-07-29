#version 460 core

in vec2 uv;

out vec4 frag_colour;

uniform sampler2D scene_texture;
uniform sampler2D depth_texture;

// https://www.youtube.com/watch?v=5xUT5QdkPAU&ab_channel=SuboptimalEngineer
vec4 box_blur() {
    vec2 resolution = vec2(1280, 360);
    vec2 texel_size = 1 / resolution;

    float kernal_size = 1;
    float blur_divisor = 9;

    vec3 box_blur_colour = vec3(0.0);
    for (float i = -kernal_size; i <= kernal_size; i++) {
        for (float j = -kernal_size; j <= kernal_size; j++) {
            vec4 neighbour = texture(scene_texture, uv + vec2(i, j) * texel_size);
            box_blur_colour += neighbour.rgb;
        }
    }

    box_blur_colour /= blur_divisor;
    return vec4(box_blur_colour, 1);
}

vec4 vignette(vec4 start_colour) {
    float cutoff = 0.7;
    vec2 ndc_uv = (uv * 2) - 1; // [-1..1]

    float distance = length(ndc_uv - vec2(0));
    if (distance > cutoff) {
        float edge_distance = 1 - (distance - cutoff);
        return vec4(edge_distance, edge_distance, edge_distance, 1) * start_colour;
    }

    return start_colour;
}

void main()
{
    float blur_depth_cutoff = 0.2;
    vec4 depth_value = texture(depth_texture, uv);

    if (depth_value.r < blur_depth_cutoff) {
        frag_colour = box_blur();
    } else {
        frag_colour = texture(scene_texture, uv);
    }

    frag_colour = vignette(frag_colour);
}
