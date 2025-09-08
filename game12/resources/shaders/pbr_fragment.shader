#version 460 core

struct PointLight {
    vec3 position;
    vec3 colour;
    float distance;
};

// keep in sync with renderer 
#define MAX_POINT_LIGHTS 20

layout(location = 0) out vec4 colour_attachment;

in vec3 fragment_position;
in vec3 normal;
in vec2 uv;

uniform vec3        camera_position;

uniform vec4        colour;

uniform vec3        ambient_light;
uniform vec3        sun_position;
uniform vec3        sun_colour;

uniform vec2        material_tiling_factor;
uniform sampler2D   material_albedo;
uniform sampler2D   material_normal;
uniform sampler2D   material_ambient_occlusion;

uniform int         light_count;
uniform PointLight  lights[MAX_POINT_LIGHTS];

vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
} 

void main() {
    vec4 fragment_colour;

    {
        vec4 albedo_sample = texture(material_albedo, uv * material_tiling_factor);
        vec4 ambient_occlusion_sample = texture(material_ambient_occlusion, uv * material_tiling_factor);

        if (albedo_sample.a == 0) {
            discard;
        }

        fragment_colour = (albedo_sample * colour) * ambient_occlusion_sample;
    }

    vec3 fragment_lighting;

    { // PBR
        vec3 N = normalize(normal);
        vec3 V = normalize(camera_position - fragment_position);

        // total incoming radiance
        vec3 irradiance = vec3(0);

        for (int i = 0; i < light_count; i++) {
            PointLight light = lights[i];
            float light_intensity = 10; // TODO: make this configurable

            vec3 light_direction = normalize(light.position - fragment_position);
            float light_distance = length(light.position - fragment_position);

            float cosTheta = max(dot(N, light_direction), 0.0);
            float attenuation = 1.0 / (light_distance * light_distance);

            vec3 radiance = light.colour * attenuation * cosTheta * light_intensity;

            irradiance += radiance;
        }
    
        fragment_lighting = ambient_light + irradiance;
    }

    if (false) { // draw normals unlit 
        fragment_lighting = vec3(1);
        fragment_colour = vec4(normal, 1);
    }

    if (false) { // draw position unlit 
        fragment_lighting = vec3(1);
        fragment_colour = vec4(fragment_position.rgb, 1);
    }

    colour_attachment = fragment_colour * vec4(fragment_lighting, 1);
} 

#if 0
    { // Blinn-Phong
        vec3 diffuse_light = diffuse_calculation(fragment_position, normal);
        vec3 point_light = point_light_calculation(fragment_position);

        fragment_lighting = ambient_light + diffuse_light + point_light;
    }
#endif

