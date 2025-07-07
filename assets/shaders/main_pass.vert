#version 460

#extension GL_EXT_buffer_reference : require
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference2 : require
#include "common.glsl"

layout(set = 1, binding = 0) uniform CameraUBO {
    Camera camera;
};

layout(set = 2, binding = 1) readonly buffer MeshBuffer {
    GpuMesh gpuMeshes[];
};

layout(set = 3, binding = 0) readonly buffer InstanceBuffer {
    Instance instances[];
};

layout(location = 0) out vec3 vertNormal;
layout(location = 1) out vec3 fragPos;
layout(location = 2) out vec2 uv;
layout(location = 3) flat out uint objectIndex;

void main() {
    Instance instance = instances[gl_InstanceIndex];
    objectIndex = GetObjectIndex(instance);
    mat4 instanceMatrix = GetInstanceMatrix(instance);
    Vertex vertex = gpuMeshes[objectIndex].vertexBuffer.vertices[gl_VertexIndex];
    vec4 worldPos = instanceMatrix * vec4(vertex.position, 1.0);

    vec4 pos = camera.projection * camera.view * worldPos;

    gl_Position = pos;

    mat3 normalMatrix = transpose(inverse(mat3(instanceMatrix)));

    vertNormal = normalMatrix * vertex.normal;

    fragPos = vec3(worldPos);

    uv = vec2(vertex.uv_x, vertex.uv_y);
}
