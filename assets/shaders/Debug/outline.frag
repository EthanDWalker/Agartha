#version 450
#extension GL_GOOGLE_include_directive : require
#include "../common.glsl"

layout(location = 0) out vec4 oColor;

void main() {
    oColor = vec4(1.0, 0.6, 0.0, 1.0);
}
