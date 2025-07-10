#version 450

layout(set = 0, binding = 2) readonly buffer InstanceColors {
    vec4 colors[];
};

layout(location = 0) in flat uint oInstanceIndex;

layout(location = 0) out vec4 oColor;

void main() {
    oColor = colors[oInstanceIndex];
}
