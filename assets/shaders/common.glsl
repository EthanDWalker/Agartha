#extension GL_EXT_buffer_reference : require

struct SvoNode {
    uint color;
    uint normal;
};

layout(buffer_reference) buffer SvoNodeBuffer {
    SvoNode nodes[];
};

struct SvoData {
    vec3 leftBound;
    float worldToSvo;
    vec3 rightBound;
    uint depth;
};

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
    float near;
    float far;
};

struct PointLight {
    vec3 color;
    float intensity;
    vec3 position;
    uint shadow_map_index;
};

struct DirectionalLight {
    vec3 color;
    float intensity;
    vec3 direction;
    uint cascade_index;
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

vec2 OctEncodeNormal(vec3 n) {
    n /= (abs(n.x) + abs(n.y) + abs(n.z));
    vec2 e = n.xy;
    if (n.z < 0.0) {
        e = (1.0 - abs(e.yx)) * sign(e);
    }
    return e * 0.5 + 0.5;
}

vec3 OctDecodeNormal(vec2 e) {
    e = e * 2.0 - 1.0;
    vec3 n = vec3(e.xy, 1.0 - abs(e.x) - abs(e.y));
    if (n.z < 0.0) {
        n.xy = (1.0 - abs(n.yx)) * sign(n.xy);
    }
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

layout(buffer_reference) readonly buffer VertexBuffer {
    Vertex vertices[];
};

struct GpuMesh {
    VertexBuffer vertexBuffer;
    uint firstIndex;
    uint indexCount;
};
