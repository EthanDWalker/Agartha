#version 450

#extension GL_GOOGLE_include_directive : require
#include "common.glsl"

layout(push_constant) uniform constants
{
    VertexBuffer vertexBuffer;
    InstanceIndicesBuffer instanceIndicesBuffer;
};

layout(set = 0, binding = 0) uniform LightMatrixUBO {
    mat4 lightMatrix;
};

layout(set = 1, binding = 0) readonly buffer instanceBuffer {
  mat4 instances[];
};

void main() {
    mat4 instanceMatrix = instances[instanceIndicesBuffer.indices[gl_InstanceIndex]];
    gl_Position = lightMatrix * instanceMatrix * vec4(vertexBuffer.vertices[gl_VertexIndex].position, 1.0);
}
