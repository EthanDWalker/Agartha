#version 450
#extension GL_GOOGLE_include_directive : require
#include "common.glsl"

layout(set = 0, binding = 0, rgba32ui) uniform readonly uimage2D gbufferImage;
layout(set = 0, binding = 1) uniform sampler2D depthImage;

layout(set = 1, binding = 0) uniform CameraUBO {
    Camera camera;
};

layout(location = 0) out vec4 oColor;

layout(location = 0) in vec2 iUV;

const float near = 0.1;
const float far = 10000.0;

vec2 UnpackMetallicRoughness(uint packed);

vec3 UnpackNormal(uint packed);

vec3 UnpackHDRColor(uint packed);

vec3 PositionFromDepth(float depth);

float LinearizeDepth(float depth);

void main() {
    uvec3 gbuffer = imageLoad(gbufferImage, ivec2(gl_FragCoord.xy)).rgb;
    vec3 color = UnpackHDRColor(gbuffer.r);

    oColor = vec4(color, 1.0);
}

vec3 PositionFromDepth(float depth) {
    float z = depth * 2.0 - 1.0;

    vec4 clipSpacePosition = vec4(iUV * 2.0 - 1.0, z, 1.0);
    vec4 viewSpacePosition = camera.invProj * clipSpacePosition;

    viewSpacePosition /= viewSpacePosition.w;

    return viewSpacePosition.xyz;
}

float LinearizeDepth(float depth) {
    return (near * far) / (far + depth * (near - far));
}

vec3 UnpackHDRColor(uint packed) {
    float r = float(packed & 0x7FF) / 32.0;
    float g = float((packed >> 11) & 0x3FF) / 64.0;
    float b = float((packed >> 21) & 0x7FF) / 32.0;

    return vec3(r, g, b);
}

vec3 UnpackNormal(uint packed) {
    float x = float(packed & 0x7FF) / 2047.0;
    float y = float((packed >> 11) & 0x3FF) / 1023.0;
    float z = float((packed >> 21) & 0x7FF) / 2047.0;

    return vec3(x, y, z) * 2.0 - 1.0;
}

// x is m and y is r
vec2 UnpackMetallicRoughness(uint packed) {
    float roughness = float(packed & 0xFFFF) / 65535.0;
    float metalness = float((packed >> 16) & 0xFFFF) / 65535.0;

    return vec2(metalness, roughness);
}
