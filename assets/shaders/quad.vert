#version 450
#extension GL_GOOGLE_include_directive : require
#include "common.glsl"

layout(location = 0) out vec2 oUV;

const vec2 quad[] = {
        vec2(-1.0, 1.0),
        vec2(1.0, 1.0),
        vec2(-1.0, -1.0),
        vec2(1.0, 1.0),
        vec2(1.0, -1.0),
        vec2(-1.0, -1.0),
    };

void main() {
    vec2 position = quad[gl_VertexIndex];
    gl_Position = vec4(position, 0.0, 1.0);
    oUV = position * 0.5 + 0.5;
}
