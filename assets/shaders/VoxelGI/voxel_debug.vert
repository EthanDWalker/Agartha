#version 450
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference2 : require
#include "../common.glsl"

layout(set = 0, binding = 1) uniform SvoDataUbo {
    SvoData svoData;
};

layout(set = 1, binding = 0) uniform CameraUBO {
    Camera camera;
};

layout(location = 0) out vec3 oWorldPos;

layout(push_constant) uniform PushConstants {
    uint level;
};

void main() {
    uint index = uint(gl_VertexIndex);

    float pointCount = pow(2, level) + 1.0;
    uint n = uint(pointCount);
    uint x = index % n;
    uint y = (index / n) % n;
    uint z = (index / (n * n));

    vec3 fraction = vec3(x, y, z) / float(n - 1);
    vec3 localPos = (fraction * 2.0 - 1.0);
    localPos = clamp(localPos, vec3(-0.9999), vec3(0.9999));

    oWorldPos = localPos.xyz * svoData.leftBound;
}
