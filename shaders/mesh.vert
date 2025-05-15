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

//push constants block
layout( push_constant ) uniform constants
{
	VertexBuffer vertexBuffer;
} PushConstants;

void main() {
    gl_Position = vec4(PushConstants.vertexBuffer.vertices[gl_VertexIndex].position, 1.0);
}
