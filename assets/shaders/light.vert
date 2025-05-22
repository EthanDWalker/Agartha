#version 450

#extension GL_EXT_buffer_reference : require
#extension GL_GOOGLE_include_directive : require
#include "common.glsl"

layout(push_constant) uniform constants
{
  VertexBuffer vertexBuffer;
  InstanceBuffer instanceBuffer;
};

layout(location = 0) out vec4 vertColor;

void main() {
    gl_Position = vec4(1.0);//FIX ME * vec4(vertexBuffer.vertices[gl_VertexIndex].position, 1.0);
    vertColor = vertexBuffer.vertices[gl_VertexIndex].color;
}
