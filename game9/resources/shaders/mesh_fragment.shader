#version 460 core

in vec3 fragment_position;
in vec3 normal;
in vec4 colour;
in vec2 uv;
in vec2 normal_uv;

layout(location = 0) out vec4 frag_colour;
layout(location = 1) out vec4 normal_colour;

uniform sampler2D atlas_texture;
uniform vec4 ambient_light;
uniform vec3 sun_direction;
uniform vec4 sun_colour;

void main()
{
    normal_colour = vec4(0, 0, 1, 1);

    vec4 sample_colour = texture(atlas_texture, uv) * colour;

    // remove if 0 alpha so the empty pixels in the texture
    // dont add redundent info to the depth buffer and cover
    // things they shouldn't
    if(sample_colour.a == 0) {
        discard;
    }

    vec4 diffuse_light = sun_colour * max(dot(normal, sun_direction), 0);

    float specularStrength = 0.6;
    // view-space so viewer is always at (0,0,0), so viewDir is (0,0,0) - Position => -Position
    vec3 viewDir = normalize(-fragment_position); 
    vec3 reflectDir = reflect(-sun_direction, normal);  
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
    vec4 specular = specularStrength * spec * sun_colour; 

    frag_colour = sample_colour * (ambient_light + diffuse_light + specular);
    normal_colour = texture(atlas_texture, normal_uv);
} 
