#version 460
#extension GL_EXT_ray_tracing : enable
#extension GL_GOOGLE_include_directive : require
#include "../common.glsl"

struct Payload {
    uint outInstanceIndex;
};

layout(location = 0) rayPayloadInEXT Payload payload;

void main() {
  payload.outInstanceIndex = 0;
}
