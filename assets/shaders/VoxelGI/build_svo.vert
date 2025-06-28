#version 450
#extension GL_EXT_buffer_reference : require
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference2 : require
#include "../common.glsl"

layout(set = 0, binding = 5) uniform SvoUbo {
    SvoData svoData;
};

layout(set = 1, binding = 0) readonly buffer InstanceBuffer {
    Instance instances[];
};

layout(set = 2, binding = 1) readonly buffer MeshBuffer {
    GpuMesh gpuMeshes[];
};

layout(location = 0) out OUT {
    vec3 worldPos;
    vec3 normal;
    uint objectIndex;
    vec2 uv;
} VS_OUT;

void main() {
    Instance instance = instances[gl_InstanceIndex];
    uint objectIndex = GetObjectIndex(instance);
    mat4 instanceMatrix = GetInstanceMatrix(instance);
    Vertex vertex = gpuMeshes[objectIndex].vertexBuffer.vertices[gl_VertexIndex];
    vec4 worldPos = instanceMatrix * vec4(vertex.position, 1.0);

    mat3 normalMatrix = transpose(inverse(mat3(instanceMatrix)));
    vec3 extent = svoData.leftBound + svoData.leftBound;

    VS_OUT.normal = normalMatrix * vertex.normal;
    VS_OUT.uv = vec2(vertex.uv_x, vertex.uv_y);
    VS_OUT.objectIndex = objectIndex;
    VS_OUT.worldPos = worldPos.xyz;

    gl_Position = vec4((worldPos.xyz) * svoData.worldToSvo, 1.0);
}
