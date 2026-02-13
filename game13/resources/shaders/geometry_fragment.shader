#version 460 core

in vec3 normal;
in vec4 colour;

layout(location = 0) out vec4 g_albedo;

void main() {
    float lighting = 0.0;

    if (normal.y != 0) {
        if (normal.y > 0) {
            lighting = 1.0; // top face
        } else {
            lighting = 0.2; // bottom face
        }
    } else {
        if (normal.z > 0 || normal.x > 0) {
            lighting = 0.8; // +z +x face
        } else {
            lighting = 0.6; // -z -x face
        }
    }

    g_albedo = vec4(colour.r * lighting, colour.g * lighting, colour.b * lighting, colour.a);
} 
