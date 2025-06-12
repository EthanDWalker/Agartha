#version 450

#extension GL_EXT_buffer_reference : require
#extension GL_GOOGLE_include_directive : require
#include "common.glsl"

layout(set = 0, binding = 0) readonly buffer VisibileInstances {
    uint visibleInstances[];
};

layout(set = 1, binding = 0) uniform CameraUBO {
    Camera camera;
};

layout(set = 2, binding = 1) readonly buffer MeshBuffer {
    GpuMesh gpuMeshes[];
};

layout(set = 3, binding = 0) readonly buffer InstanceBuffer {
    Instance instances[];
};

const float near = 0.1;
const float far = 10000.0;

float LinearizeDepth(float depth) {
    return (near * far) / (far + depth * (near - far));
}

void main() {
    Instance instance = instances[visibleInstances[gl_InstanceIndex]];
    mat4 instanceMatrix = instance.matrix;
    Vertex vertex = gpuMeshes[instance.objectIndex].vertexBuffer.vertices[gl_VertexIndex];
    vec4 worldPos = instanceMatrix * vec4(vertex.position, 1.0);

    vec4 pos = camera.projection * camera.view * worldPos;

    gl_Position = pos;
    gl_Position.z = LinearizeDepth(gl_Position.z);
}
