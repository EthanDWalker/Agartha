#version 450

#extension GL_GOOGLE_include_directive : require
#include "common.glsl"

layout(binding = 0) uniform CameraUBO {
    Camera camera;
};

layout(lines) in;
layout(line_strip, max_vertices = 24) out;

void EmitLine(vec3 a, vec3 b) {
    gl_Position = camera.projection * camera.view * vec4(a, 1.0);
    float bias = 0.0001;
    gl_Position -= vec4(0.0, 0.0, bias, 0.0);
    EmitVertex();
    gl_Position = camera.projection * camera.view * vec4(b, 1.0);
    EmitVertex();
    EndPrimitive();
}

void main() {
    vec3 min = gl_in[0].gl_Position.xyz;
    vec3 max = gl_in[1].gl_Position.xyz;

    // Define 8 corners of the AABB
    vec3 c000 = vec3(min.x, min.y, min.z);
    vec3 c001 = vec3(min.x, min.y, max.z);
    vec3 c010 = vec3(min.x, max.y, min.z);
    vec3 c011 = vec3(min.x, max.y, max.z);
    vec3 c100 = vec3(max.x, min.y, min.z);
    vec3 c101 = vec3(max.x, min.y, max.z);
    vec3 c110 = vec3(max.x, max.y, min.z);
    vec3 c111 = vec3(max.x, max.y, max.z);

    // 12 edges of the box
    EmitLine(c000, c001);
    EmitLine(c001, c011);
    EmitLine(c011, c010);
    EmitLine(c010, c000);

    EmitLine(c100, c101);
    EmitLine(c101, c111);
    EmitLine(c111, c110);
    EmitLine(c110, c100);

    EmitLine(c000, c100);
    EmitLine(c001, c101);
    EmitLine(c011, c111);
    EmitLine(c010, c110);
}
