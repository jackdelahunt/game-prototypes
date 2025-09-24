#version 460 core

layout(location = 0) out vec4 colour_attachment;

void main() {
    float depth = gl_FragCoord.z;

    if (false) {
        depth = (depth * 2) - 1;
    }

    colour_attachment = vec4(depth, depth, depth, 1);
} 
