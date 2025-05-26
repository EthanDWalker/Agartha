#version 450

#extension GL_EXT_buffer_reference : require
#extension GL_GOOGLE_include_directive : require
#include "common.glsl"

layout(push_constant) uniform constants
{
    VertexBuffer vertexBuffer;
    InstanceBuffer instanceBuffer;
};

layout(std140, binding = 2) uniform CameraUBO {
    Camera camera;
};

layout(binding = 8) uniform LightMatrixUBO {
    mat4 lightMatrix;
};

layout(location = 0) out vec3 vertColor;
layout(location = 1) out vec3 vertNormal;
layout(location = 2) out vec3 fragPos;
layout(location = 3) out vec2 uv;
layout(location = 4) out vec4 fragPosLight;

void main() {
    mat4 instanceMatrix = instanceBuffer.instances[gl_InstanceIndex];
    Vertex vertex = vertexBuffer.vertices[gl_VertexIndex];
    vec4 worldPos = instanceMatrix * vec4(vertex.position, 1.0);

    gl_Position = camera.projection * camera.view * worldPos;

    vertColor = vertex.color.xyz;

    mat3 normalMatrix = transpose(inverse(mat3(instanceMatrix)));

    vertNormal = normalMatrix * vertex.normal;

    fragPos = vec3(worldPos);

    fragPosLight = lightMatrix * worldPos;

    uv = vec2(vertex.uv_x, vertex.uv_y);
}
