#version 450

#extension GL_EXT_buffer_reference : require
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference2 : require
#include "common.glsl"

layout(set = 0, binding = 0) readonly buffer VisibleInstanceBuffer {
    uint visibleInstances[];
};

layout(set = 1, binding = 0) readonly buffer InstanceBuffer {
    Instance instances[];
};

layout(set = 2, binding = 1) readonly buffer MeshBuffer {
    GpuMesh gpuMeshes[];
};

layout(set = 3, binding = 2) uniform LightMatrixUBO {
    mat4 lightMatrix;
};


void main() {
    Instance instance = instances[visibleInstances[gl_InstanceIndex]];
    uint objectIndex = GetObjectIndex(instance);
    mat4 instanceMatrix = GetInstanceMatrix(instance);
    Vertex vertex = gpuMeshes[objectIndex].vertexBuffer.vertices[gl_VertexIndex];
    vec4 worldPos = instanceMatrix * vec4(vertex.position, 1.0);

    gl_Position = lightMatrix * worldPos;
}
