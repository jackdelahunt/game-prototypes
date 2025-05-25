#version 460 core

struct Light {
    vec2 position;
    float radius;
    vec4 colour;
    float intensity;
};

in vec2 uv;

out vec4 frag_colour;

uniform vec4 global_light;

// rgb -> xyz
// [0..1] -> [-1..1]
uniform sampler2D normals_texture;
uniform sampler2D scene_texture;
uniform sampler2D depth_texture;

uniform int light_count;
uniform Light lights[20];

uniform float aspect_ratio; // width / height

void main()
{
    // convert uv coord to ndc and then scale based on aspect ratio
    vec2 uv_to_ndc = (uv * 2) - 1;
    vec2 scaled_ndc = vec2(uv_to_ndc.x * aspect_ratio, uv_to_ndc.y);

    vec4 base_colour = texture(scene_texture, uv);
    vec4 accumulated_light = vec4(0);

    float depth = texture(depth_texture, uv).r;
    if (depth >= 0.99) {
        frag_colour = base_colour * global_light;
        return;
    }

    // get normal from normal map and transform so each
    // channel is normalized between -1 and 1
    vec3 normal = texture(normals_texture, uv).rgb;
    normal = normalize((normal * 2) - 1);

    for(int i = 0; i < light_count; i++) {
        Light light = lights[i];

        vec2 scaled_light_position = light.position * vec2(aspect_ratio, 1);
        vec2 light_direction = scaled_light_position - scaled_ndc;
        float distance = length(light_direction);

        if (distance < light.radius) {
            float influence = 1.0 - (distance / light.radius);

            vec3 norm_light_direction = normalize(vec3(light_direction, 0));
            float NdotL = max(dot(normal, norm_light_direction), 0.1);

            accumulated_light += light.colour * influence * light.intensity * NdotL;
        }
    }

    vec4 total_light = global_light + accumulated_light;
    total_light = clamp(total_light, 0.0, 1.0); // prevent oversaturation
    
    frag_colour = base_colour * total_light;
}
