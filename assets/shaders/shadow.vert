#version 450

#extension GL_GOOGLE_include_directive : require
#include "common.glsl"

layout(push_constant) uniform constants
{
    VertexBuffer vertexBuffer;
    InstanceBuffer instanceBuffer;
};

layout(binding = 0) uniform LightMatrixUBO {
    mat4 lightMatrix;
};

void main() {
    mat4 instanceMatrix = instanceBuffer.instances[gl_InstanceIndex];
    gl_Position = lightMatrix * instanceMatrix * vec4(vertexBuffer.vertices[gl_VertexIndex].position, 1.0);
}
