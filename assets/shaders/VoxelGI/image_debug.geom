#version 450
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_nonuniform_qualifier : require
#include "../common.glsl"

layout(points) in;
layout(triangle_strip, max_vertices = 24) out;

layout(set = 0, binding = 0, rgba16) readonly uniform image3D radianceImageMips[];

layout(set = 0, binding = 1) uniform SvoDataUbo {
    SvoData svoData;
};

layout(set = 1, binding = 0) uniform CameraUBO {
    Camera camera;
};

layout(push_constant) uniform PushConstants {
    uint mipLevel;
};

layout(location = 0) in vec3 iWorldPos[];

layout(location = 0) out vec3 oColor;

void EmitFace(vec3 center, vec3 halfSize, int axis, float sign, vec3 color) {
    vec3 normal = vec3(0.0);
    normal[axis] = sign;

    vec3 up = vec3(0.0);
    up[(axis + 1) % 3] = 1.0;
    vec3 right = cross(normal, up);

    vec3 p0 = center + normal * halfSize[axis] - right * halfSize[(axis + 2) % 3] - up * halfSize[(axis + 1) % 3];
    vec3 p1 = center + normal * halfSize[axis] + right * halfSize[(axis + 2) % 3] - up * halfSize[(axis + 1) % 3];
    vec3 p2 = center + normal * halfSize[axis] - right * halfSize[(axis + 2) % 3] + up * halfSize[(axis + 1) % 3];
    vec3 p3 = center + normal * halfSize[axis] + right * halfSize[(axis + 2) % 3] + up * halfSize[(axis + 1) % 3];

    oColor = color;
    gl_Position = camera.projection * camera.view * vec4(p0, 1.0);
    EmitVertex();
    oColor = color;
    gl_Position = camera.projection * camera.view * vec4(p1, 1.0);
    EmitVertex();
    oColor = color;
    gl_Position = camera.projection * camera.view * vec4(p2, 1.0);
    EmitVertex();
    oColor = color;
    gl_Position = camera.projection * camera.view * vec4(p3, 1.0);
    EmitVertex();

    EndPrimitive();
}

void main() {
    vec3 center = iWorldPos[0];
    vec4 color = imageLoad(radianceImageMips[mipLevel],
            ivec3((center + svoData.leftBound) / float(svoData.voxelSize * pow(2, mipLevel))));
    if (color.a == 0.0) {
        return;
    }

    float cubesPerAxis = imageSize(radianceImageMips[mipLevel]).x;
    vec3 halfSize = svoData.leftBound / cubesPerAxis;

    for (int axis = 0; axis < 3; ++axis) {
        EmitFace(center, halfSize, axis, 1.0, color.rgb);
        EmitFace(center, halfSize, axis, -1.0, color.rgb);
    }
}
