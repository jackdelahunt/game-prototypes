#version 460 core

layout(location = 0) out vec4 g_position;
layout(location = 1) out vec4 g_normal;
layout(location = 2) out vec4 g_view_normal;
layout(location = 3) out vec4 g_albedo;
layout(location = 4) out vec4 g_sun_position;

void main() {
    g_position = vec4(1, 1, 1, 1);
} 
