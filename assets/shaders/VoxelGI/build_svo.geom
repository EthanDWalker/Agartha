#version 450

layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;

layout(location = 0) in IN {
    vec3 worldPos;
    vec3 normal;
    uint objectIndex;
    vec2 uv;
} GS_IN[];

layout(location = 0) out OUT {
    vec3 position;
    vec3 worldPos;
    uint objectIndex;
    vec3 normal;
    vec2 uv;
} GS_OUT;

vec2 Project(vec3 vertex, uint axis) {
    return axis == 0 ? vertex.yz : (axis == 1 ? vertex.xz : vertex.xy);
}

vec3 BiasAndScale(vec3 vertex) {
    return (vertex + 1.0) * 0.5;
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
        gl_ViewportIndex = axis;
        gl_Position = vec4(Project(gl_in[i].gl_Position.xyz, axis), 1.0, 1.0);
        GS_OUT.position = BiasAndScale(gl_in[i].gl_Position.xyz);
        GS_OUT.normal = GS_IN[i].normal;
        GS_OUT.objectIndex = GS_IN[i].objectIndex;
        GS_OUT.uv = GS_IN[i].uv;
        GS_OUT.worldPos = GS_IN[i].worldPos;
        EmitVertex();
    }
    EndPrimitive();
}
