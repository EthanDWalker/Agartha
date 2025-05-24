#version 450

#extension GL_EXT_buffer_reference : require
#extension GL_GOOGLE_include_directive : require
#include "common.glsl"

layout(push_constant) uniform constants
{
  VertexBuffer vertexBuffer;
  InstanceBuffer instanceBuffer;
};

layout(std140, binding = 0) uniform CameraUBO {
  Camera camera;
};

layout(location = 0) out vec3 vertColor;
layout(location = 1) out vec2 uv;

void main() {
    mat4 instanceMatrix = instanceBuffer.instances[gl_InstanceIndex];
    Vertex vertex = vertexBuffer.vertices[gl_VertexIndex];

    // Extract world-space position of this instance from matrix
    vec3 instancePosition = vec3(instanceMatrix[3]);

    // Compute view-facing direction (Z axis of billboard)
    vec3 forward = normalize(camera.viewPos - instancePosition);

    // Arbitrary "up" direction
    vec3 up = vec3(0.0, 1.0, 0.0);

    // Construct right and actual up vectors to form rotation basis
    vec3 right = normalize(cross(up, forward));
    vec3 adjustedUp = cross(forward, right);

    // Construct a rotation matrix (mat3) to align quad to face the viewPos
    mat3 faceRotation = mat3(right, adjustedUp, forward);

    // Transform the vertex position from local quad space → face-aligned → world
    vec3 rotatedPosition = faceRotation * vertex.position.xyz;
    vec4 worldPosition = instanceMatrix * vec4(0.0, 0.0, 0.0, 1.0);  // origin of the instance
    vec3 finalWorldPos = worldPosition.xyz + rotatedPosition;

    // Output final screen-space position
    gl_Position = (camera.projection * camera.view * vec4(finalWorldPos.xyz, 1.0)) * vec4(1.0, 1.0, 0.0, 1.0);

    vertColor = vertex.color.rgb;
    uv = vec2(vertex.uv_x, 1 - vertex.uv_y);
}
