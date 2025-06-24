#version 460 core

// layout(location = 0) out vec4 g_position;
// layout(location = 1) out vec4 g_normal;
// layout(location = 2) out vec4 g_view_normal;
// layout(location = 3) out vec4 g_albedo;
// layout(location = 4) out vec4 g_sun_position;

in vec3 normal;
in vec2 texture_uv;

out vec4 frag;

uniform sampler2D albedo;

void main() {
    frag = texture(albedo, texture_uv);
} 
