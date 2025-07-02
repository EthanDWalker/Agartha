#version 450
#extension GL_EXT_buffer_reference : require
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference2 : require
#include "../common.glsl"

layout(set = 0, binding = 1) uniform SvoDataUbo {
    SvoData svoData;
};

layout(set = 1, binding = 0) readonly buffer InstanceBuffer {
    Instance instances[];
};

layout(set = 2, binding = 1) readonly buffer MeshBuffer {
    GpuMesh gpuMeshes[];
};

layout(location = 0) out vec3 iWorldPos;
layout(location = 1) out vec3 iNormal;
layout(location = 2) out vec2 iUv;
layout(location = 3) flat out uint iObjectIndex;

void main() {
    Instance instance = instances[gl_InstanceIndex];
    uint objectIndex = GetObjectIndex(instance);
    mat4 instanceMatrix = GetInstanceMatrix(instance);
    Vertex vertex = gpuMeshes[objectIndex].vertexBuffer.vertices[gl_VertexIndex];
    vec4 worldPos = instanceMatrix * vec4(vertex.position, 1.0);

    mat3 normalMatrix = transpose(inverse(mat3(instanceMatrix)));

    iNormal = normalMatrix * vertex.normal;
    iUv = vec2(vertex.uv_x, vertex.uv_y);
    iObjectIndex = objectIndex;
    iWorldPos = worldPos.xyz;

    gl_Position = vec4((worldPos.xyz) * svoData.worldToSvo, 1.0);
}
