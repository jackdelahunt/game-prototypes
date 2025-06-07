#version 460 core

in vec2 uv;

layout(location = 0) out vec4 g_position;

uniform sampler2D position_map;
uniform sampler2D normal_map;
uniform sampler2D albedo_map;
uniform sampler2D sun_position_map;
uniform sampler2D shadow_map;
uniform sampler2D ssao_map;

uniform vec3 ambient_light;
uniform vec3 sun_position;
uniform vec3 sun_colour;

vec3 diffuse_calculation(vec3 position, vec3 normal) {
    vec3 sun_direction = normalize(sun_position - position);
    float diffuse = max(dot(sun_direction, normal), 0);

    return sun_colour * diffuse;
}


vec3 specular_calculation(vec3 position, vec3 normal) {
    float specularStrength = 0.6;

    vec3 viewDir = normalize(-position); 
    vec3 sun_direction = normalize(sun_position - position);
    vec3 reflectDir = reflect(-sun_direction, normal);  

    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
    vec3 specular = specularStrength * spec * sun_colour; 

    return specular;
}

float shadow_calculation(vec3 fragment_position, vec3 fragment_normal, vec4 fragPosLightSpace)
{
    vec3 shadow_coords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    shadow_coords = shadow_coords * 0.5 + 0.5; 

    float closest_depth = texture(shadow_map, shadow_coords.xy).r;
    float current_depth = shadow_coords.z;

    vec3 sun_direction = normalize(sun_position - fragment_position);
    float bias = max(0.05 * (1.0 - dot(fragment_normal, sun_direction)), 0.005);  

#if 0
    float shadow = current_depth - bias > closest_depth  ? 1.0 : 0.0;
#else
    float shadow = 0.0;
    vec2 texel_size = 1.0 / textureSize(shadow_map, 0);
    for(int x = -1; x <= 1; ++x)
    {
        for(int y = -1; y <= 1; ++y)
        {
            float pcfDepth = texture(shadow_map, shadow_coords.xy + vec2(x, y) * texel_size).r; 
            shadow += current_depth - bias > pcfDepth  ? 1.0 : 0.0;        
        }    
    }
    shadow /= 9.0;
#endif

    return shadow;
}

void main()
{
    vec3 fragment_position = texture(position_map, uv).rgb;
    vec3 fragment_normal = texture(normal_map, uv).rgb;
    vec3 fragment_albedo = texture(albedo_map, uv).rgb;
    vec4 fragment_sun_position = texture(sun_position_map, uv);
    float fragment_ssao = length(texture(ssao_map, uv).rgb);

    if (length(fragment_position) < 0.1) {
        discard;
    }

    vec3 diffuse_light = diffuse_calculation(fragment_position, fragment_normal); 
    vec3 specular_light = specular_calculation(fragment_position, fragment_normal);
    float shadow = shadow_calculation(fragment_position, fragment_normal, fragment_sun_position);

    vec3 lighting = (ambient_light * fragment_ssao) + (1.0 - shadow) * (diffuse_light + specular_light);

    g_position = vec4(fragment_albedo * lighting, 1);
} 
