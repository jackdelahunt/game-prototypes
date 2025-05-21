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

vec4 vingette() {
}

void main()
{
    float blur_depth_cutoff = 0.2;
    vec4 depth_value = texture(depth_texture, uv);

    if (depth_value.r >= blur_depth_cutoff) {
        frag_colour = texture(scene_texture, uv);
        return;
    }

    frag_colour = box_blur();
}
