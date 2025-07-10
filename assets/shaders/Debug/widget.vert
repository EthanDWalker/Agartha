#version 450

#extension GL_EXT_buffer_reference : require
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference2 : require
#include "../common.glsl"

layout(set = 0, binding = 0) readonly buffer InstanceBuffer {
    mat4 instanceMatrices[];
};

layout(set = 0, binding = 1) readonly buffer VerticeBuffer {
    Vertex vertices[];
};

layout(set = 1, binding = 0) uniform CameraUBO {
    Camera camera;
};

layout(push_constant) uniform PushConstants {
    mat4 widgetMatrix;
};

layout(location = 0) out flat uint oInstanceIndex;

void main() {
    mat4 instanceMatrix = instanceMatrices[gl_InstanceIndex];
    vec4 worldPos = instanceMatrix * vec4(vertices[gl_VertexIndex].position, 1.0);

    vec4 pos = camera.projection * camera.view * (widgetMatrix * worldPos);

    gl_Position = pos;
    oInstanceIndex = gl_InstanceIndex;
}
