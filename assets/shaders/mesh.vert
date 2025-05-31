#version 460

#extension GL_EXT_buffer_reference : require
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference2 : require
#include "common.glsl"

layout(set = 0, binding = 4) uniform LightMatrixUBO {
    mat4 lightMatrix;
};

layout(set = 0, binding = 5) readonly buffer VisibileInstances {
    uint visibleInstances[];
};

layout(set = 2, binding = 0) uniform CameraUBO {
    Camera camera;
};

layout(set = 3, binding = 1) readonly buffer MeshBuffer {
    GpuMesh gpuMeshes[];
};

layout(set = 4, binding = 0) readonly buffer InstanceBuffer {
    Instance instances[];
};

layout(location = 0) out vec3 vertColor;
layout(location = 1) out vec3 vertNormal;
layout(location = 2) out vec3 fragPos;
layout(location = 3) out vec2 uv;
layout(location = 4) out vec4 fragPosLight;
layout(location = 5) flat out uint objectIndex;

void main() {
    Instance instance = instances[visibleInstances[gl_DrawID]];
    objectIndex = instance.objectIndex;
    mat4 instanceMatrix = instance.matrix;
    Vertex vertex = gpuMeshes[instance.objectIndex].vertexBuffer.vertices[gl_VertexIndex];
    vec4 worldPos = instanceMatrix * vec4(vertex.position, 1.0);

    gl_Position = camera.projection * camera.view * worldPos;

    vertColor = vertex.color.xyz;

    mat3 normalMatrix = transpose(inverse(mat3(instanceMatrix)));

    vertNormal = normalMatrix * vertex.normal;

    fragPos = vec3(worldPos);

    fragPosLight = lightMatrix * worldPos;

    uv = vec2(vertex.uv_x, vertex.uv_y);
}
