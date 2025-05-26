#extension GL_EXT_buffer_reference : require

struct Camera {
    mat4 view;
    mat4 projection;
    vec3 viewPos;
};

struct PointLight {
    vec4 color;
    vec3 position;
};

struct DirectionalLight {
    vec3 direction;
};

struct Vertex {
    vec3 position;
    float uv_x;
    vec3 normal;
    float uv_y;
    vec4 color;
};

struct Material {
    int albedo;
    int metal_roughness;
    int emissive;
    int normal;
    int ambient_occlusion;
};

layout(buffer_reference, std430) readonly buffer VertexBuffer {
    Vertex vertices[];
};

layout(buffer_reference, std430) readonly buffer InstanceBuffer {
    mat4 instances[];
};
