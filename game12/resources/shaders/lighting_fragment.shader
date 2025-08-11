#version 460 core

in vec2 uv;

layout(location = 0) out vec4 g_position;

uniform sampler2D position_map;
uniform sampler2D normal_map;
uniform sampler2D albedo_map;

uniform vec3 ambient_light;
uniform vec3 sun_position;
uniform vec3 sun_colour;
uniform vec3 shadow_colour;

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

void main()
{
    vec3 fragment_position = texture(position_map, uv).rgb;
    if (length(fragment_position) == 0) {
        discard;
    }

    vec3 fragment_normal = texture(normal_map, uv).rgb;
    vec3 fragment_albedo = texture(albedo_map, uv).rgb;

    vec3 diffuse_light = diffuse_calculation(fragment_position, fragment_normal); 

    vec3 lighting = ambient_light + diffuse_light;
    g_position = vec4(fragment_albedo * lighting, 1);
} 
