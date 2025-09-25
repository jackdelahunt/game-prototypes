#version 460 core

// keep in sync with renderer 
#define SSAO_KERNAL_SAMPLES 64

layout(location = 0) out vec4 colour_attachment;

in vec2 uv;

uniform sampler2D position_map;
uniform sampler2D normal_map;
uniform sampler2D noise_map;

uniform mat4 projection;

uniform float radius;
uniform float bias;
uniform vec2 noise_scale;

uniform vec3 samples[SSAO_KERNAL_SAMPLES];

void main()
{
    vec3 fragPos   = texture(position_map, uv).xyz;
    vec3 normal    = normalize(texture(normal_map, uv).rgb);
    vec3 randomVec = normalize(texture(noise_map, uv * noise_scale).xyz);

    vec3 tangent   = normalize(randomVec - normal * dot(randomVec, normal));
    vec3 bitangent = cross(normal, tangent);
    mat3 TBN       = mat3(tangent, bitangent, normal);  

    float occlusion = 0.0;
    for(int i = 0; i < SSAO_KERNAL_SAMPLES; ++i)
    {
        vec3 samplePos = TBN * samples[i]; // from tangent to view-space
        samplePos = fragPos + samplePos * radius; 

        vec4 offset = vec4(samplePos, 1.0);
        offset      = projection * offset;    // from view to clip-space
        offset.xy /= offset.w;               // perspective divide
        offset.xy  = offset.xy * 0.5 + 0.5; // transform to range 0.0 - 1.0  

        float sampleDepth = texture(position_map, offset.xy).z;

        float rangeCheck = abs(fragPos.z - sampleDepth) < radius ? 1.0 : 0.0;
        occlusion += (sampleDepth <= samplePos.z + bias ? 1.0 : 0.0) * rangeCheck;
    }

    occlusion = 1.0 - (occlusion / SSAO_KERNAL_SAMPLES);

    colour_attachment = vec4(occlusion, occlusion, occlusion, 1);
} 
