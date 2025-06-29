#version 450
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference2 : require
#include "../common.glsl"

layout(set = 0, binding = 0) readonly buffer SvoBuffer {
    SvoNodeBuffer svo[];
};

layout(set = 0, binding = 1) uniform SvoDataUbo {
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
    uint levelIndex = 0;
    vec3 levelPosition = svo;
    for (uint i = 0; i < level; ++i) {
        vec3 roundedPosition = floor(levelPosition * 2.0);
        levelIndex *= 8;
        levelIndex += uint(roundedPosition.x * 1) + uint(roundedPosition.y * 2) + uint(roundedPosition.z * 4);
        levelPosition = (levelPosition - (vec3(0.5) * roundedPosition)) * 2.0;
    }
    return levelIndex;
}

void main() {
    Instance instance = instances[gl_InstanceIndex];
    uint objectIndex = GetObjectIndex(instance);
    mat4 instanceMatrix = GetInstanceMatrix(instance);
    Vertex vertex = gpuMeshes[objectIndex].vertexBuffer.vertices[gl_VertexIndex];
    vec4 worldPos = instanceMatrix * vec4(vertex.position, 1.0);

    uint svoLevel = svoData.depth;
    SvoNode node = svo[svoLevel - 1].nodes[GetNodeIndex(worldPos.xyz, svoLevel)];
    vec3 nodeColor = unpackUnorm4x8(node.color).rgb;
    if ((node.color & 0x1) == 0x0) {
        nodeColor = vec3(1.0, 0.0, 1.0);
    }

    gl_Position = camera.projection * camera.view * worldPos;
    color = nodeColor;
}
