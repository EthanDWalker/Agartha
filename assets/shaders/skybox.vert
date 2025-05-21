#version 450

#extension GL_EXT_buffer_reference : require

layout(location = 0) out vec3 localPos;

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
    mat4 projection;
    mat4 view;
    VertexBuffer vertexBuffer;
};

void main()
{
    localPos = vertexBuffer.vertices[gl_VertexIndex].position;
    mat4 rotView = mat4(mat3(view));
    vec4 clipPos = projection * view * vec4(localPos * 500, 1.0);
    gl_Position = clipPos.xyww; // so it always is at the back
}
