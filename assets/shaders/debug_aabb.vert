#version 450

#extension GL_EXT_buffer_reference : require
#extension GL_GOOGLE_include_directive : require
#include "common.glsl"

layout(push_constant) uniform constants {
    AabbBuffer aabb_buffer;
};

void main() {
    AABB aabb = aabb_buffer.aabbs[gl_VertexIndex / 2];
    if (gl_VertexIndex % 2 == 0) {
        gl_Position = vec4(aabb.min, 1.0);
    } else {
        gl_Position = vec4(aabb.max, 1.0);
    }
}
