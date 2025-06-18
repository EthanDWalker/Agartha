#version 460
#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_ray_tracing_position_fetch : enable
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_nonuniform_qualifier : require
#include "common.glsl"

layout(location = 0) rayPayloadInEXT uint hitInstanceIndex;

hitAttributeEXT vec2 attribs;

void main() {
  hitInstanceIndex = gl_InstanceID + 1;
}
