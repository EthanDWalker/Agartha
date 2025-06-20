#version 460

#extension GL_EXT_buffer_reference : require
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference2 : require
#include "common.glsl"

layout(set = 0, binding = 1) readonly buffer RayQueryResults {
    uint rayResults[];
};

layout(set = 1, binding = 0) readonly buffer InstanceBuffer {
    Instance instances[];
};

layout(set = 2, binding = 1) readonly buffer MeshBuffer {
    GpuMesh gpuMeshes[];
};

layout(set = 3, binding = 0) uniform CameraUBO {
    Camera camera;
};

layout(location = 0) out vec3 oWorld;
layout(location = 1) out vec3 oNormal;

void main() {
    Instance instance = instances[gl_InstanceIndex];
    GpuMesh mesh = gpuMeshes[GetObjectIndex(instance)];
    mat4 instanceMatrix = GetInstanceMatrix(instance);
    Vertex vertex = mesh.vertexBuffer.vertices[gl_VertexIndex];
    oWorld = vec3(instanceMatrix * vec4(vertex.position, 1.0));
    gl_Position = camera.projection * camera.view * vec4(oWorld, 1.0);

    mat3 normalMatrix = transpose(inverse(mat3(instanceMatrix)));

    oNormal = normalMatrix * vertex.normal;
}
