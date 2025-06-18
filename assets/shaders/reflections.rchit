#version 460
#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_ray_tracing_position_fetch : enable
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_samplerless_texture_functions : require
#include "common.glsl"

struct Payload {
    vec3 outColor;
    vec3 inWorldPosition;
};

layout(location = 0) rayPayloadInEXT Payload payload;

layout(set = 0, binding = 4) uniform sampler2D shadowMap;

layout(set = 1, binding = 0) uniform CameraUBO {
    Camera camera;
};

layout(set = 2, binding = 0) readonly buffer Objects {
    Object objects[];
};

layout(set = 2, binding = 1) readonly buffer GpuMeshes {
    GpuMesh gpuMeshes[];
};

layout(set = 2, binding = 3) readonly buffer IndexBuffer {
    uint indices[];
};

layout(set = 3, binding = 0) uniform texture2D textures[];

layout(set = 3, binding = 1) uniform sampler textureSampler;

layout(set = 4, binding = 0) uniform PointLightBuffer {
    PointLight pointLight;
};

layout(std140, set = 4, binding = 1) uniform DirectionalLightBuffer {
    DirectionalLight directionalLight;
};

layout(std140, set = 4, binding = 2) uniform LightMatrixBuffer {
    mat4 lightMatrix;
};

hitAttributeEXT vec2 attribs;

const float PI = 3.14159265359;

vec3 GetNormalFromMap(uint materialIndex, vec3 normal, vec2 uv);

float DistributionGGX(vec3 N, vec3 H, float roughness);

float GeometrySchlickGGX(float NdotV, float roughness);
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness);

vec3 FresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness);

float ShadowCalculation(vec3 L, vec3 N, vec4 lightSpace);

void main() {
    GpuMesh mesh = gpuMeshes[gl_InstanceCustomIndexEXT];
    Object object = objects[gl_InstanceCustomIndexEXT];

    uint indexOffset = gl_PrimitiveID / 3;

    VertexBuffer vertexBuffer = mesh.vertexBuffer;
    Vertex v0 = vertexBuffer.vertices[indices[mesh.firstIndex + indexOffset + 0]];
    Vertex v1 = vertexBuffer.vertices[indices[mesh.firstIndex + indexOffset + 1]];
    Vertex v2 = vertexBuffer.vertices[indices[mesh.firstIndex + indexOffset + 2]];

    vec2 uv0 = vec2(v0.uv_x, v0.uv_y);
    vec2 uv1 = vec2(v1.uv_x, v1.uv_y);
    vec2 uv2 = vec2(v2.uv_x, v2.uv_y);

    vec3 barycentric = vec3(1.0f - attribs.x - attribs.y, attribs.x, attribs.y);

    vec2 uv = uv0 * barycentric.x + uv1 * barycentric.y + uv2 * barycentric.z;
    vec3 normal = v0.normal * barycentric.x + v1.normal * barycentric.y + v2.normal * barycentric.z;
    vec3 position0 = gl_HitTriangleVertexPositionsEXT[0];
    vec3 position1 = gl_HitTriangleVertexPositionsEXT[1];
    vec3 position2 = gl_HitTriangleVertexPositionsEXT[2];

    vec3 position = position0 * barycentric.x + position1 * barycentric.y + position2 * barycentric.z;

    vec3 worldPosition = gl_ObjectToWorldEXT * vec4(position, 1.0);

    vec4 lightSpace = lightMatrix * vec4(worldPosition, 1.0);

    vec3 albedo = texture(sampler2D(textures[object.material.albedo], textureSampler), uv).rgb;
    vec3 mr = texture(sampler2D(textures[object.material.metal_roughness], textureSampler), uv).rgb;
    float metallic = mr.b;
    float roughness = mr.g;
    vec3 emisive = texture(sampler2D(textures[object.material.emissive], textureSampler), uv).rgb;
    float ao = texture(sampler2D(textures[object.material.ambient_occlusion], textureSampler), uv).r;

    vec3 N = GetNormalFromMap(object.material.normal, normal, uv);
    vec3 V = normalize(camera.viewPos - worldPosition);
    vec3 R = reflect(-V, N);

    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metallic);

    vec3 Lo = vec3(0.0);

    {
        vec3 L = normalize(pointLight.position - worldPosition);
        vec3 H = normalize(V + L);

        float distance = length(pointLight.position - worldPosition);
        float attenuation = 1.0 / (distance * distance);
        vec3 radiance = pointLight.color.xyz * attenuation * pointLight.color.w;

        float NDF = DistributionGGX(N, H, roughness);
        float G = GeometrySmith(N, V, L, roughness);
        vec3 F = FresnelSchlickRoughness(max(dot(N, V), 0.0), F0, roughness);

        vec3 numerator = NDF * G * F;
        float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
        vec3 specular = numerator / denominator;

        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;

        kD *= 1.0 - metallic;

        float NdotL = max(dot(N, L), 0.0);

        Lo += (kD * albedo / PI + specular) * radiance * NdotL;
    }

    {
        vec3 L = normalize(-directionalLight.direction.xyz);
        vec3 H = normalize(V + L);

        vec3 radiance = vec3(1.0, 0.8, 0.5);

        float NDF = DistributionGGX(N, H, roughness);
        float G = GeometrySmith(N, V, L, roughness);
        vec3 F = FresnelSchlickRoughness(max(dot(N, V), 0.0), F0, roughness);

        vec3 numerator = NDF * G * F;
        float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
        vec3 specular = numerator / denominator;

        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;

        kD *= 1.0 - metallic;

        float NdotL = max(dot(N, L), 0.0);

        float shadow = ShadowCalculation(L, N, lightSpace) + .05;

        Lo += shadow * (kD * albedo / PI + specular) * radiance * NdotL;
    }

    vec3 F = FresnelSchlickRoughness(max(dot(N, V), 0.0), F0, roughness);

    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metallic;

    vec3 color = Lo + emisive + (F + albedo) * kD * ao;

    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0 / 2.2));

    payload.outColor = color;
}

vec3 GetNormalFromMap(uint materialIndex, vec3 normal, vec2 uv) {
    vec3 tangentNormal = texture(sampler2D(textures[materialIndex], textureSampler), uv).xyz * 2.0 - 1.0;

    vec3 Q1 = payload.inWorldPosition;
    vec3 Q2 = payload.inWorldPosition;
    vec2 st1 = uv;
    vec2 st2 = uv;

    vec3 N = normalize(normal);
    vec3 T = normalize(Q1 * st2.t - Q2 * st1.t);
    vec3 B = -normalize(cross(N, T));
    mat3 TBN = mat3(T, B, N);

    return normalize(TBN * tangentNormal);
}

// Approximates the number subsurface mircofacets that align with the half way ray
float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return num / denom;
}

// Approximates how much of the subsurace mircofacets are shadowed by others
float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

    float num = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return num / denom;
}
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

// Approximate how much light is refracted vs light reflected
// vec3 F0 = vec3(0.04); // Base reflectivity
// F0      = mix(F0, surfaceColor.rgb, metalness);
// cosTheta is dot product between normal and halfway(or view dir)
vec3 FresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness) {
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

float ShadowCalculation(vec3 L, vec3 N, vec4 lightSpace) {
    vec3 projCoords = lightSpace.xyz / lightSpace.w;

    projCoords = projCoords * 0.5 + 0.5;

    if (projCoords.z > 1.0 || projCoords.x < 0.0 || projCoords.x > 1.0 || projCoords.y < 0.0 || projCoords.y > 1.0)
        return 1.0;

    float closestDepth = texture(shadowMap, projCoords.xy).r;

    float currentDepth = projCoords.z;

    currentDepth = (1.0 - currentDepth) * 2;

    float bias = max(0.05 * (1.0 - dot(N, L)), 0.005);

    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadowMap, 0);

    for (int x = -1; x <= 1; ++x)
    {
        for (int y = -1; y <= 1; ++y)
        {
            float pcfDepth = texture(shadowMap, projCoords.xy + vec2(x, y) * texelSize).r;
            shadow += currentDepth + bias < pcfDepth ? 0.0 : 1.0;
        }
    }
    shadow /= 9.0;

    return shadow;
}
