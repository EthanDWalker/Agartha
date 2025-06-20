#version 460
#extension GL_GOOGLE_include_directive : require
#include "common.glsl"

layout(location = 0) out vec4 oColor;

layout(location = 0) in vec3 iNormal;
layout(location = 1) in vec3 iWorld;

layout(set = 3, binding = 0) uniform CameraUBO {
    Camera camera;
};

void main() {
    vec3 V = normalize(camera.viewPos - iWorld);
    oColor = vec4(1.0);
    return;
    if (dot(iNormal, V) > .9) {} else {
        discard;
    }
}
