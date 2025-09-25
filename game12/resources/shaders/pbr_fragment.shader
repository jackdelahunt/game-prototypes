#version 460 core

struct PointLight {
    vec3 position;
    vec3 colour;
    float intensity;
};

// keep in sync with renderer 
#define MAX_POINT_LIGHTS 50

#define MAX_SHADOW_SAMPLE_BIAS  0.005
#define MIN_SHADOW_SAMPLE_BIAS  0.001
#define SHADOW_PCF_RADIUS       2

#define PI 3.14159265359
 
layout(location = 0) out vec4 colour_attachment;
layout(location = 1) out vec4 position_attachment;
layout(location = 2) out vec4 normal_attachment;

in vec3 world_fragment_position;
in vec3 view_fragment_position;
in vec3 world_fragment_normal;
in vec3 view_fragment_normal;
in vec2 model_fragment_uv;
in vec4 sun_fragment_position;

uniform vec3        camera_position;

uniform vec4        colour;

uniform vec3        ambient_light;
uniform vec3        sun_position;
uniform vec3        sun_colour;
uniform float       sun_intensity;

uniform vec2        material_tiling_factor;
uniform int         material_triplanar_enabled;
uniform float       material_triplanar_scale;
uniform sampler2D   material_albedo;
uniform sampler2D   material_normal;
uniform sampler2D   material_ambient_occlusion;
uniform sampler2D   material_roughness;
uniform sampler2D   material_metalness;

uniform sampler2D   shadow_map;

uniform int         light_count;
uniform PointLight  lights[MAX_POINT_LIGHTS];

vec3 get_fragment_normal(vec3 normal_sample) {
    vec3 Q1  = dFdx(world_fragment_position);
    vec3 Q2  = dFdy(world_fragment_position);
    vec2 st1 = dFdx(model_fragment_uv);
    vec2 st2 = dFdy(model_fragment_uv);

    vec3 N   = normalize(world_fragment_normal);
    vec3 T  = normalize(Q1*st2.t - Q2*st1.t);
    vec3 B  = -normalize(cross(world_fragment_normal, T));
    mat3 TBN = mat3(T, B, world_fragment_normal);

    return normalize(TBN * normal_sample);
}

float BRDF_distribution(vec3 N, vec3 H, float roughness) {
    float a      = roughness * roughness;
    float a2     = a*a;
    float NdotH  = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;
	
    float num   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
	
    return num / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float num   = NdotV;
    float denom = NdotV * (1.0 - k) + k;
	
    return num / denom;
}

float BRDF_geometry(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2  = GeometrySchlickGGX(NdotV, roughness);
    float ggx1  = GeometrySchlickGGX(NdotL, roughness);
	
    return ggx1 * ggx2;
}

vec3 BRDF_fresnel(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec4 triplanar(sampler2D texture_sampler, float scale) {
    // Absolute normal for blend weights
    vec3 blending = abs(normalize(world_fragment_normal));
    // Make sure weights sum to 1
    blending = blending / (blending.x + blending.y + blending.z);

    // Scale world position to control tiling
    vec3 wp = world_fragment_position * scale;

    // Sample texture projected onto each axis plane
    vec4 xProj = texture(texture_sampler, wp.yz); // project along X axis
    vec4 yProj = texture(texture_sampler, wp.xz); // project along Y axis
    vec4 zProj = texture(texture_sampler, wp.xy); // project along Z axis

    // Blend the three projections
    return xProj * blending.x + yProj * blending.y + zProj * blending.z;
}

float get_fragment_shadow() {
    // perform perspective divide
    vec3 shadow_map_position = sun_fragment_position.xyz / sun_fragment_position.w;
    shadow_map_position = shadow_map_position * 0.5 + 0.5;

    float closestDepth = texture(shadow_map, shadow_map_position.xy).r;
    float currentDepth = shadow_map_position.z;

    vec3 sun_direction = normalize(sun_position);

    // add a bias to the sampling to reduce shadow acne
    float bias = max(MAX_SHADOW_SAMPLE_BIAS * (1.0 - dot(world_fragment_normal, sun_direction)), MIN_SHADOW_SAMPLE_BIAS);

    // samples is the total number of pixels to be sampled based on the radius
    float pcf_samples = (SHADOW_PCF_RADIUS * 2) + 1;
    pcf_samples *= pcf_samples;

    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadow_map, 0);
    for(int x = -SHADOW_PCF_RADIUS; x <= SHADOW_PCF_RADIUS; ++x)
    {
        for(int y = -SHADOW_PCF_RADIUS; y <= SHADOW_PCF_RADIUS; ++y)
        {
            float pcfDepth = texture(shadow_map, shadow_map_position.xy + vec2(x, y) * texelSize).r; 
            shadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0;        
        }    
    }

    shadow /= pcf_samples;

    return shadow; 
}

void main() {
    vec3 albedo_sample              = vec3(0);
    vec3 normal_sample              = vec3(0);
    float ambient_occlusion_sample  = 0;
    float roughness_sample          = 0;
    float metalness_sample          = 0;

    if (material_triplanar_enabled == 1) {
        vec4 albedo = triplanar(material_albedo, material_triplanar_scale) * colour;
        if (albedo.a == 0) {
            discard;
        }
    
        albedo_sample           = albedo.rgb;
        normal_sample           = (triplanar(material_normal, material_triplanar_scale).xyz * 2.0) - 1.0;
        ambient_occlusion_sample= (triplanar(material_ambient_occlusion, material_triplanar_scale)).r;
        roughness_sample        = (triplanar(material_roughness, material_triplanar_scale)).r;
        metalness_sample        = (triplanar(material_metalness, material_triplanar_scale)).r;
    } 
    else {
        vec2 scaled_model_fragment_uv = model_fragment_uv * material_tiling_factor;

        vec4 albedo = texture(material_albedo, scaled_model_fragment_uv) * colour;
        if (albedo.a == 0) {
            discard;
        }
    
        albedo_sample           = albedo.rgb;
        normal_sample           = (texture(material_normal,             scaled_model_fragment_uv).xyz * 2.0) - 1.0;
        ambient_occlusion_sample= (texture(material_ambient_occlusion,  scaled_model_fragment_uv)).r;
        roughness_sample        = (texture(material_roughness,          scaled_model_fragment_uv)).r;
        metalness_sample        = (texture(material_metalness,          scaled_model_fragment_uv)).r;
    }

    { // PBR
        vec3 N = get_fragment_normal(normal_sample);
        vec3 V = normalize(camera_position - world_fragment_position);

        vec3 F0 = vec3(0.04); 
        F0 = mix(F0, albedo_sample, metalness_sample);

        vec3 Lo = vec3(0);

        { // directional light
            vec3 L = normalize(sun_position);
            vec3 H = normalize(V + L);

            float attenuation = 1.0;
            float shadow = get_fragment_shadow();

            vec3 radiance = sun_colour * (attenuation * sun_intensity * (1 - shadow));

            float D = BRDF_distribution(N, H, roughness_sample);
            float G = BRDF_geometry(N, V, L, roughness_sample);
            vec3 F  = BRDF_fresnel(max(dot(H, V), 0.0), F0);

            vec3 numerator    = D * G * F;
            float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0)  + 0.0001;
            vec3 specular     = numerator / denominator;

            vec3 kS = F;
            vec3 kD = vec3(1.0) - kS;
  
            kD *= 1.0 - metalness_sample;

            float NdotL = max(dot(N, L), 0.0);        
            Lo += (kD * albedo_sample / PI + specular) * radiance * NdotL;
        }

        for (int i = 0; i < light_count; i++) {
            PointLight light = lights[i];

            vec3 L = normalize(light.position - world_fragment_position);
            vec3 H = normalize(V + L);

            float light_distance = length(light.position - world_fragment_position);
            float attenuation = 1.0 / (light_distance * light_distance);
            vec3 radiance = light.colour * attenuation * light.intensity;

            float D = BRDF_distribution(N, H, roughness_sample);
            float G = BRDF_geometry(N, V, L, roughness_sample);
            vec3 F  = BRDF_fresnel(max(dot(H, V), 0.0), F0);

            vec3 numerator    = D * G * F;
            float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0)  + 0.0001;
            vec3 specular     = numerator / denominator;

            vec3 kS = F;
            vec3 kD = vec3(1.0) - kS;
  
            kD *= 1.0 - metalness_sample;

            float NdotL = max(dot(N, L), 0.0);        
            Lo += (kD * albedo_sample / PI + specular) * radiance * NdotL;
        }

        vec3 colour = (ambient_light * albedo_sample * ambient_occlusion_sample) + Lo;

        colour = colour / (colour + vec3(1.0)); // tone mapping     HDR -> LDR
        colour = pow(colour, vec3(1.0/2.2));    // gamma correction RGB -> sRGB

        // debug draw normals
        // colour = (N + 1) * 0.5;

        // debug draw shadows
        // float shadow = get_fragment_shadow();
        // colour = vec3(1 - shadow);

        // debug draw sun position
        // colour = sun_fragment_position.rgb;

        colour_attachment = vec4(colour, 1);
        position_attachment = vec4(view_fragment_position, 1);
        normal_attachment = vec4(view_fragment_normal, 1);
    }
} 
