#version 450

layout(location = 0) in vec3 iUV;

layout(set = 0, binding = 1) uniform samplerCube skyboxTexture;

layout(location = 0) out vec4 oColor;

void main() {
    oColor = vec4(texture(skyboxTexture, normalize(iUV)).rgb, 1.0);
}
