#version 460 core

layout(location = 0) out vec4 colour_attachment;

struct PointLight {
    vec3 position;
    vec3 colour;
    float distance;
};

// needs to be kept in sync if renderer 
#define MAX_POINT_LIGHTS 20

in vec3 fragment_position;
in vec3 normal;
in vec2 uv;

uniform vec4 colour;

uniform vec3        ambient_light;
uniform vec3        sun_position;
uniform vec3        sun_colour;

uniform vec2        material_tiling_factor;
uniform sampler2D   material_albedo;
uniform sampler2D   material_ambient_occlusion;

uniform int         light_count;
uniform PointLight  lights[MAX_POINT_LIGHTS];

vec3 diffuse_calculation(vec3 position, vec3 normal) {
    vec3 sun_direction = normalize(sun_position - position);

    float diffuse = dot(sun_direction, normal);
    return sun_colour * diffuse;
}

float ease_in_quint(float x) {
    return x * x * x * x * x;
}

vec3 point_light_calculation(vec3 position) {
    vec3 total_light = vec3(0);

    for (int i = 0; i < light_count; i++) {
        PointLight light = lights[i];

        float distance = length(light.position - position);
        float influence = 1 - (distance / light.distance);
    
        if (influence < 0) {
            influence = 0;
        }
    
        influence = ease_in_quint(influence);
    
        total_light += light.colour * influence; 
    }

    return total_light;
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

    {
        vec3 diffuse_light = diffuse_calculation(fragment_position, normal);
        vec3 point_light = point_light_calculation(fragment_position);

        fragment_lighting = ambient_light + diffuse_light + point_light;
    }


    colour_attachment = fragment_colour * vec4(fragment_lighting, 1);
} 
