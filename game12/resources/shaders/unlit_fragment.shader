#version 460 core

layout(location = 0) out vec4 colour_attachment;

in vec3 normal;
in vec2 uv;

uniform vec4        colour;

uniform vec2        material_tiling_factor;
uniform sampler2D   material_albedo;

void main() {
    vec4 albedo = texture(material_albedo, uv * material_tiling_factor) * colour;
    if (albedo.a == 0) {
        discard;
    }

    vec3 colour = albedo.rgb;
    colour = colour / (colour + vec3(1.0)); // tone mapping     HDR -> LDR
    colour = pow(colour, vec3(1.0/2.2));    // gamma correction RGB -> sRGB
    colour_attachment = vec4(colour, 1);
} 
