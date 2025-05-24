#version 450

layout(binding = 1) uniform sampler2D image;

layout(location = 0) in vec3 vertColor;
layout(location = 1) in vec2 uv;

layout(location = 0) out vec4 outColor;
void main() {
  outColor = texture(image, uv);
  outColor = vec4(1.0, 0.0, 0.0, 0.3);
}
