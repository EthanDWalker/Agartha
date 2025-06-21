#extension GL_EXT_buffer_reference : require

struct Frustum {
    vec4 top;
    vec4 bottom;
    vec4 right;
    vec4 left;
    vec4 far;
    vec4 near;
};

struct Camera {
    mat4 view;
    mat4 projection;
    mat4 invView;
    mat4 invProj;
    vec3 viewPos;
    float padding;
    Frustum frustum;
};

struct PointLight {
    vec3 color;
    float intensity;
    vec3 position;
    float padding;
};

struct DirectionalLight {
    vec3 direction;
    float intensity;
};

struct Vertex {
    vec3 position;
    float uv_x;
    vec3 normal;
    float uv_y;
};

struct Instance {
    mat3x4 transform;
    uint instanceCustomIndexMask;
    uint bindingTableOffsetFlags;
    uvec2 blasAddress;
};

mat4 Mat3x4toMat4(mat3x4 m) {
    vec3 translation = vec3(m[0].w, m[1].w, m[2].w);

    mat4 matrix = mat4(
        m[0].xyz, 0.0,
        m[1].xyz, 0.0,
        m[2].xyz, 0.0,
        translation, 1.0
    );
    return matrix; 
}

uint GetObjectIndex(Instance instance) {
    return instance.instanceCustomIndexMask & 0xFFFFFF;
}

mat4 GetInstanceMatrix(Instance instance) {
    return Mat3x4toMat4(instance.transform);
}

// expects x and y
vec3 CalculateNormal(vec2 N) {
    vec3 n = vec3(N.x, N.y, 1.0 - abs(N.x) - abs(N.y));
    float t = clamp(-n.z, 0.0, 1.0);
    n.xy += (n.x >= 0.0 ? -t : t) * (n.y >= 0.0 ? 1.0 : -1.0);
    return normalize(n);
}

struct Material {
    int albedoAo;
    int mrNormal;
};

struct Object {
    Material material;
};

struct SphereBounds {
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
