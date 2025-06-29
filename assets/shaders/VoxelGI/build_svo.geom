#version 450

layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;

layout(location = 0) in vec3 iWorldPos[];
layout(location = 1) in vec3 iNormal[];
layout(location = 2) in vec2 iUv[];
layout(location = 3) flat in uint iObjectIndex[];

layout(location = 0) out vec3 oPos;
layout(location = 1) out vec3 oWorldPos;
layout(location = 2) out vec3 oNormal;
layout(location = 3) out vec2 oUv;
layout(location = 4) flat out uint oObjectIndex;

vec2 Project(vec3 vertex, uint axis) {
    return axis == 0 ? vertex.yz : (axis == 1 ? vertex.xz : vertex.xy);
}

int GetDominantAxis(vec3 pos0, vec3 pos1, vec3 pos2) {
    vec3 normal = abs(cross(pos1 - pos0, pos2 - pos0));
    return (normal.x > normal.y && normal.x > normal.z) ? 0 :
    (normal.y > normal.z) ? 1 : 2;
}

void main() {
    int axis = GetDominantAxis(gl_in[0].gl_Position.xyz,
            gl_in[1].gl_Position.xyz,
            gl_in[2].gl_Position.xyz);

    for (int i = 0; i < 3; ++i) {
        gl_Position = vec4(Project(gl_in[i].gl_Position.xyz, axis), 1.0, 1.0);
        oPos = (1.0 + gl_in[i].gl_Position.xyz) * 0.5;
        oNormal = iNormal[i];
        oObjectIndex = iObjectIndex[i];
        oUv = iUv[i];
        oWorldPos = iWorldPos[i];
        EmitVertex();
    }
    EndPrimitive();
}
