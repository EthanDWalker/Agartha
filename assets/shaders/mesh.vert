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

layout(location = 0) out vec3 vertColor;
layout(location = 1) out vec3 vertNormal;
layout(location = 2) out vec3 fragPos;
layout(location = 3) out vec2 uv;

void main() {
    mat4 instanceMatrix = instanceBuffer.instances[gl_InstanceIndex];
    Vertex vertex = vertexBuffer.vertices[gl_VertexIndex];

    gl_Position = camera.projection * camera.view * instanceMatrix * vec4(vertex.position, 1.0);

    vertColor = vertex.color.xyz;

    mat3 normalMatrix = transpose(inverse(mat3(instanceMatrix)));
    vertNormal = normalMatrix * vertex.normal;

    fragPos = vec3(instanceMatrix * vec4(vertex.position, 1.0));

    uv = vec2(vertex.uv_x, vertex.uv_y);
}
