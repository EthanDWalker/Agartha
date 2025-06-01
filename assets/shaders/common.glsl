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
};

struct Instance {
    mat4 matrix;
    vec3 color;
    uint objectIndex;
};

struct Material {
    int albedo;
    int metal_roughness;
    int emissive;
    int normal;
    int ambient_occlusion;
};

struct Object {
    Material material;
};

struct SphereBounds {
    vec3 center;
    float radius;
};

struct DrawIndexedIndirectCommand {
    uint indexCount;
    uint instanceCount;
    uint firstIndex;
    int vertexOffset;
    uint firstInstance;
};

layout(buffer_reference, std430) readonly buffer VertexBuffer {
    Vertex vertices[];
};

struct GpuMesh {
    VertexBuffer vertexBuffer;
    uint firstIndex;
    uint indexCount;
};
