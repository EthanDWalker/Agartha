#version 450
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference2 : require
#include "../common.glsl"

layout(set = 0, binding = 0) readonly buffer SVOLevel1Buffer {
    SvoNode l1Buffer[];
};

layout(set = 0, binding = 1) readonly buffer SVOLevel2Buffer {
    SvoNode l2Buffer[];
};

layout(set = 0, binding = 2) readonly buffer SVOLevel3Buffer {
    SvoNode l3Buffer[];
};

layout(set = 0, binding = 3) readonly buffer SVOLevel4Buffer {
    SvoNode l4Buffer[];
};

layout(set = 0, binding = 4) readonly buffer SVOLevel5Buffer {
    SvoNode l5Buffer[];
};

layout(set = 0, binding = 5) uniform SvoUbo {
    SvoData svoData;
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

layout(location = 0) out vec3 color;

uint GetNodeIndex(vec3 worldPosition, uint level) {
    vec3 svo = worldPosition * svoData.worldToSvo;
    svo = (svo + 1.0) * 0.5;
    uint liIndex = 0;
    vec3 liPosition = svo;
    for (uint i = 0; i < level; ++i) {
        vec3 roundedPosition = floor(liPosition * 2.0);
        liIndex *= 8;
        liIndex += uint(roundedPosition.x * 1) + uint(roundedPosition.y * 2) + uint(roundedPosition.z * 4);
        liPosition = (liPosition - (vec3(0.5) * roundedPosition)) * 2.0;
    }
    return liIndex;
}

void main() {
    Instance instance = instances[gl_InstanceIndex];
    uint objectIndex = GetObjectIndex(instance);
    mat4 instanceMatrix = GetInstanceMatrix(instance);
    Vertex vertex = gpuMeshes[objectIndex].vertexBuffer.vertices[gl_VertexIndex];
    vec4 worldPos = instanceMatrix * vec4(vertex.position, 1.0);

    SvoNode node = l5Buffer[GetNodeIndex(worldPos.xyz, 5)];
    vec3 nodeColor = node.color;
    if ((node.visible & 0x1) == 0x0) {
        nodeColor = vec3(1.0, 0.0, 1.0);
    }

    gl_Position = camera.projection * camera.view * worldPos;
    color = nodeColor;
}
