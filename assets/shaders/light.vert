#version 450

#extension GL_EXT_buffer_reference : require

struct Vertex {
	vec3 position;
	float uv_x;
	vec3 normal;
	float uv_y;
	vec4 color;
}; 

layout(buffer_reference, std430) readonly buffer VertexBuffer{ 
	Vertex vertices[];
};

layout( push_constant ) uniform constants
{
  mat4 worldMatrix;
  vec4 padding;
	VertexBuffer vertexBuffer;
} PushConstants;

layout(location = 0) out vec4 vertColor;

void main() {
    gl_Position = PushConstants.worldMatrix * vec4(PushConstants.vertexBuffer.vertices[gl_VertexIndex].position, 1.0);
    vertColor = PushConstants.vertexBuffer.vertices[gl_VertexIndex].color;
}
