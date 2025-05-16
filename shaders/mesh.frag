#version 450

layout(location = 0) out vec4 outColor;

layout( push_constant ) uniform constants
{
	float padding[2];
} PushConstants;

void main() {
    outColor = vec4(1.0, 0.0, 0.0, 1.0);
}
