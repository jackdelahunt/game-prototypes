#version 460 core

struct PointLight {
    vec3 position;
    vec3 colour;
    float intensity;
};

// keep in sync with renderer 
#define MAX_POINT_LIGHTS 50
   
const float PI = 3.14159265359;

layout(location = 0) out vec4 colour_attachment;

in vec3 fragment_position;
in vec3 normal;
in vec2 uv;

uniform vec3        camera_position;

uniform vec4        colour;

uniform vec3        ambient_light;
uniform vec3        sun_position;
uniform vec3        sun_colour;
uniform float       sun_intensity;

uniform vec2        material_tiling_factor;
uniform sampler2D   material_albedo;
uniform sampler2D   material_normal;
uniform sampler2D   material_ambient_occlusion;
uniform sampler2D   material_roughness;
uniform sampler2D   material_metalness;

uniform int         light_count;
uniform PointLight  lights[MAX_POINT_LIGHTS];

vec3 get_fragment_normal(vec3 normal_sample) {
    vec3 Q1  = dFdx(fragment_position);
    vec3 Q2  = dFdy(fragment_position);
    vec2 st1 = dFdx(uv);
    vec2 st2 = dFdy(uv);

    vec3 N   = normalize(normal);
    vec3 T  = normalize(Q1*st2.t - Q2*st1.t);
    vec3 B  = -normalize(cross(normal, T));
    mat3 TBN = mat3(T, B, normal);

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

void main() {
    vec3 albedo_sample              = (texture(material_albedo,             uv * material_tiling_factor) * colour).rgb;
    vec3 normal_sample              = (texture(material_normal,             uv * material_tiling_factor).xyz * 2.0) - 1.0;
    float ambient_occlusion_sample  = (texture(material_ambient_occlusion,  uv * material_tiling_factor)).r;
    float roughness_sample          = (texture(material_roughness,          uv * material_tiling_factor)).r;
    float metalness_sample          = (texture(material_metalness,          uv * material_tiling_factor)).r;

    { // PBR
        vec3 N = get_fragment_normal(normal_sample);
        vec3 V = normalize(camera_position - fragment_position);

        vec3 F0 = vec3(0.04); 
        F0 = mix(F0, albedo_sample, metalness_sample);

        vec3 Lo = vec3(0);

        { // directional light
            vec3 L = normalize(sun_position);
            vec3 H = normalize(V + L);

            float attenuation = 1.0;
            vec3 radiance = sun_colour * attenuation * sun_intensity;

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

            vec3 L = normalize(light.position - fragment_position);
            vec3 H = normalize(V + L);

            float light_distance = length(light.position - fragment_position);
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
    
        colour_attachment = vec4(colour, 1);
    }
} 
