#version 450

#extension GL_EXT_buffer_reference : require
#extension GL_GOOGLE_include_directive : require
#include "common.glsl"

layout(location = 0) out vec3 localPos;

layout(push_constant) uniform constants
{
    VertexBuffer vertexBuffer;
};

layout(set = 1, binding = 0) uniform CameraUBO {
  Camera camera;
};

void main()
{
    localPos = vertexBuffer.vertices[gl_VertexIndex].position;
    mat4 rotView = mat4(mat3(camera.view));
    vec4 clipPos = camera.projection * rotView * vec4(localPos, 1.0);
    gl_Position = clipPos.xyww; // so it always is at the back
}
