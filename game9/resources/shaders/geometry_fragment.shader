#version 460 core

in vec3 fragment_position;
in vec4 fragment_sun_position;
in vec3 normal;
in vec3 view_normal;
in vec4 colour;
in vec2 uv;
in vec2 normal_uv;

layout(location = 0) out vec4 g_position;
layout(location = 1) out vec4 g_normal;
layout(location = 2) out vec4 g_view_normal;
layout(location = 3) out vec4 g_albedo;
layout(location = 4) out vec4 g_sun_position;

uniform sampler2D atlas_texture;
uniform sampler2D shadow_map;

uniform vec3 ambient_light;
uniform vec3 sun_position;
uniform vec3 sun_colour;

vec3 diffuse_calculation() {
    vec3 sun_direction = normalize(sun_position - fragment_position);
    float diffuse = max(dot(sun_direction, normal), 0);

    return sun_colour * diffuse;
}

vec3 specular_calculation() {
    float specularStrength = 0.6;
    vec3 viewDir = normalize(-fragment_position); 
    vec3 sun_direction = normalize(sun_position - fragment_position);
    vec3 reflectDir = reflect(-sun_direction, normal);  
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
    vec3 specular = specularStrength * spec * sun_colour; 

    return specular;
}

float shadow_calculation(vec4 fragPosLightSpace)
{
    vec3 shadow_coords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    shadow_coords = shadow_coords * 0.5 + 0.5; 

    float closest_depth = texture(shadow_map, shadow_coords.xy).r;
    float current_depth = shadow_coords.z;

    vec3 sun_direction = normalize(sun_position - fragment_position);
    float bias = max(0.05 * (1.0 - dot(normal, sun_direction)), 0.005);  

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
    vec4 sample_colour = texture(atlas_texture, uv) * colour;

    // remove if 0 alpha so the empty pixels in the texture
    // dont add redundent info to the depth buffer and cover
    // things they shouldn't
    if(sample_colour.a == 0) {
        discard;
    }

    vec3 diffuse_light = diffuse_calculation(); 
    vec3 specular = specular_calculation();
    float shadow = shadow_calculation(fragment_sun_position);

    vec4 lighting = vec4(ambient_light + (1.0 - shadow) * (diffuse_light + specular), 1);

    g_position = vec4(fragment_position, 1);
    g_normal = vec4(normal, 1); 
    g_view_normal = vec4(normalize(view_normal), 1); 
    g_albedo = sample_colour;
    g_sun_position = fragment_sun_position;
} 
