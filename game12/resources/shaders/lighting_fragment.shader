#version 460 core

in vec2 uv;

layout(location = 0) out vec4 g_position;

struct PointLight {
    vec3 position;
    vec3 colour;
    float distance;
};

#define MAX_POINT_LIGHTS 1
#define MAX_POINT_LIGHT_DISTANCE 30

uniform sampler2D position_map;
uniform sampler2D normal_map;
uniform sampler2D albedo_map;

uniform vec3 ambient_light;
uniform vec3 sun_position;
uniform vec3 sun_colour;
uniform vec3 shadow_colour;
uniform int light_count;
uniform PointLight lights[MAX_POINT_LIGHTS];

vec3 diffuse_calculation(vec3 position, vec3 normal) {
    vec3 sun_direction = normalize(sun_position - position);

#if 0
    float diffuse = max(dot(sun_direction, normal), 0);
    return sun_colour * diffuse;
#else
    float diffuse = dot(sun_direction, normal);
    vec3 diffuse_colour = vec3(
        max(diffuse, shadow_colour.r),
        max(diffuse, shadow_colour.g),
        max(diffuse, shadow_colour.b)
    );

    return sun_colour * diffuse_colour;
#endif
}

vec3 diffuse_colour(vec3 position, vec3 normal) {
    vec3 sun_direction = normalize(sun_position - position);
    float diffuse = max(dot(sun_direction, normal), 0);

    return mix(shadow_colour, sun_colour, diffuse);
}

float ease_in_quint(float x) {
    return x * x * x * x * x;
}

vec3 point_light_calculation(vec3 frag_position) {
    vec3 total_light = vec3(0);

    for (int i = 0; i < light_count; i++) {
        PointLight light = lights[i];
        float distance = length(light.position - frag_position);
        float influence = 1 - (distance / light.distance);
    
        if (influence < 0) {
            influence = 0;
        }
    
        influence = ease_in_quint(influence);
    
        total_light += light.colour * influence; 
    }

    return total_light;
}

void main()
{
    vec3 fragment_position = texture(position_map, uv).rgb;
    if (length(fragment_position) == 0) {
        discard;
    }

    vec3 fragment_normal = texture(normal_map, uv).rgb;
    vec3 fragment_albedo = texture(albedo_map, uv).rgb;

    vec3 diffuse_light = diffuse_calculation(fragment_position, fragment_normal);
    vec3 point_light_light = point_light_calculation(fragment_position);

    vec3 lighting = ambient_light + diffuse_light + point_light_light;
    g_position = vec4(fragment_albedo * lighting, 1);
} 
