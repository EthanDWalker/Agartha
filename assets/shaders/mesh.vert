#version 450

#extension GL_EXT_buffer_reference : require

struct Vertex {
    vec3 position;
    float uv_x;
    vec3 normal;
    float uv_y;
    vec4 color;
};

layout(buffer_reference, std430) readonly buffer VertexBuffer {
    Vertex vertices[];
};

layout(push_constant) uniform constants
{
    mat4 worldMatrix;
    vec3 viewPos;
    float padding;
    VertexBuffer vertexBuffer;
} PushConstants;

layout(location = 0) out vec3 vertColor;
layout(location = 1) out vec3 vertNormal;
layout(location = 2) out vec3 fragPos;
layout(location = 3) out vec2 uv;

void main() {
    gl_Position = PushConstants.worldMatrix * vec4(PushConstants.vertexBuffer.vertices[gl_VertexIndex].position, 1.0);

    vertColor = PushConstants.vertexBuffer.vertices[gl_VertexIndex].color.xyz;

    vertNormal = PushConstants.vertexBuffer.vertices[gl_VertexIndex].normal;

    fragPos = PushConstants.vertexBuffer.vertices[gl_VertexIndex].position;

    uv = vec2(PushConstants.vertexBuffer.vertices[gl_VertexIndex].uv_x, PushConstants.vertexBuffer.vertices[gl_VertexIndex].uv_y);
}
