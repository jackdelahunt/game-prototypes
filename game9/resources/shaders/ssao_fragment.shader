#version 460 core

#define SSAO_SAMPLES 48

in vec2 uv;

layout(location = 0) out vec4 frag_colour;

uniform sampler2D position_map;
uniform sampler2D normal_map;
uniform sampler2D noise_map;

uniform mat4 projection;

uniform vec3 samples[SSAO_SAMPLES];

uniform float radius;
uniform float bias;
uniform vec2 noise_scale;

void main()
{
    vec3 fragPos   = texture(position_map, uv).xyz;
    vec3 normal    = texture(normal_map, uv).rgb;
    vec3 randomVec = texture(noise_map, uv * noise_scale).xyz;

    vec3 tangent   = normalize(randomVec - normal * dot(randomVec, normal));
    vec3 bitangent = cross(normal, tangent);
    mat3 TBN       = mat3(tangent, bitangent, normal);  

    float occlusion = 0.0;
    for(int i = 0; i < SSAO_SAMPLES; ++i)
    {
        vec3 samplePos = TBN * samples[i]; // from tangent to view-space
        samplePos = fragPos + samplePos * radius; 

        vec4 offset = vec4(samplePos, 1.0);
        offset      = projection * offset;    // from view to clip-space
        offset.xyz /= offset.w;               // perspective divide
        offset.xyz  = offset.xyz * 0.5 + 0.5; // transform to range 0.0 - 1.0  

        float sampleDepth = texture(position_map, offset.xy).z;

#if 1
        occlusion += (sampleDepth >= samplePos.z + bias ? 1.0 : 0.0);
#else
        float rangeCheck = smoothstep(0.0, 1.0, radius / abs(fragPos.z - sampleDepth));
        occlusion += (sampleDepth >= samplePos.z + bias ? 1.0 : 0.0) * rangeCheck;
#endif
    }

    occlusion = 1 - (occlusion / SSAO_SAMPLES);

    frag_colour = vec4(occlusion, occlusion, occlusion, 1);
} 
