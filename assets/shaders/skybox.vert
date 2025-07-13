#version 450
#extension GL_GOOGLE_include_directive : require
#include "common.glsl"

const vec3 cubePositions[] = {
        vec3(-1, -1, -1),
        vec3(1, -1, -1),
        vec3(1, 1, -1),
        vec3(-1, 1, -1),
        vec3(-1, -1, 1),
        vec3(1, -1, 1),
        vec3(1, 1, 1),
        vec3(-1, 1, 1),
    };

uint cubeIndices[36] = 
{
    0, 1, 3, 3, 1, 2,
    1, 5, 2, 2, 5, 6,
    5, 4, 6, 6, 4, 7,
    4, 0, 7, 7, 0, 3,
    3, 2, 7, 7, 2, 6,
    4, 5, 0, 0, 5, 1,
};

layout(location = 0) out vec3 oUV;

layout(set = 1, binding = 0) uniform CameraUBO {
    Camera camera;
};

void main() {
    gl_Position = camera.projection * camera.view * (vec4(cubePositions[cubeIndices[gl_VertexIndex]] * 500.0, 1.0));
    oUV = normalize(cubePositions[cubeIndices[gl_VertexIndex]]);
}
