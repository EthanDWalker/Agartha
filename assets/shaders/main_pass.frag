#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_samplerless_texture_functions : require
#include "common.glsl"

layout(location = 0) in vec3 iNormal;
layout(location = 1) in vec3 iWorldPos;
layout(location = 2) in vec2 iUV;
layout(location = 4) flat in uint iObjectIndex;

layout(location = 0) out vec4 oColor;
layout(location = 1) out vec4 oMrNormal;

layout(set = 1, binding = 0) uniform texture2D textures[];

layout(set = 1, binding = 1) uniform sampler textureSampler;

layout(set = 2, binding = 0) uniform CameraUBO {
    Camera camera;
};

layout(set = 3, binding = 0) readonly buffer ObjectBuffer {
    Object objects[];
};

layout(set = 5, binding = 0) readonly buffer PointLightBuffer {
    PointLight pointLight;
};

layout(set = 5, binding = 1) readonly buffer DirectionalLightBuffer {
    DirectionalLight directionalLight;
};

layout(set = 6, binding = 0) uniform texture2D shadowMaps[];

layout(set = 6, binding = 1) readonly buffer LightMatrixBuffer {
    mat4 lightMatrices[];
};

layout(set = 6, binding = 2) uniform sampler shadowSampler;

const float PI = 3.14159265359;

vec3 GetNormalFromMap(vec3 sampledNormal);

float DistributionGGX(vec3 N, vec3 H, float roughness);

float GeometrySchlickGGX(float NdotV, float roughness);
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness);

vec3 FresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness);

float ShadowCalculation(vec3 L, vec3 N);

vec2 OctEncodeNormal(vec3 n);

void main() {
    Material mat = objects[iObjectIndex].material;

    vec4 albedoAo = texture(sampler2D(textures[mat.albedoAo], textureSampler), iUV);
    vec4 mrNormal = texture(sampler2D(textures[mat.mrNormal], textureSampler), iUV);
    float metallic = clamp(mrNormal.x, 0.0, 1.0);
    float roughness = clamp(mrNormal.y, 0.0, 1.0);

    vec3 normal = vec3(mrNormal.zw, 0.0);
    normal.z = sqrt(1.0 - clamp(dot(normal.xy, normal.xy), 0.0, 1.0));

    vec3 albedo = albedoAo.rgb;
    float ao = albedoAo.a;

    vec3 N = GetNormalFromMap(normal);
    vec3 V = normalize(camera.viewPos - iWorldPos);
    vec3 R = reflect(-V, N);

    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metallic);

    vec3 Lo = vec3(0.0);

    {
        vec3 L = normalize(pointLight.position - iWorldPos);
        vec3 H = normalize(V + L);

        float distance = length(pointLight.position - iWorldPos);
        float attenuation = 1.0 / (distance * distance);
        vec3 radiance = pointLight.color.xyz * attenuation * pointLight.intensity;

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
        vec3 L = normalize(-directionalLight.direction);
        vec3 H = normalize(V + L);

        vec3 radiance = vec3(1.0, 0.9, 0.8) * directionalLight.intensity;

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

        float shadow = ShadowCalculation(L, N) + .05;

        Lo += shadow * (kD * albedo / PI + specular) * radiance * NdotL;
    }

    vec3 F = FresnelSchlickRoughness(max(dot(N, V), 0.0), F0, roughness);

    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metallic;

    vec3 color = Lo + (F + albedo) * kD * ao;

    oColor = vec4(color, 1.0);

    vec2 packedNormal = OctEncodeNormal(N);

    oMrNormal = vec4(metallic, roughness, packedNormal.x, packedNormal.y);
}

vec2 OctEncodeNormal(vec3 n) {
    n /= (abs(n.x) + abs(n.y) + abs(n.z)); // project onto octahedron
    vec2 e = n.xy;
    if (n.z < 0.0) {
        e = (1.0 - abs(e.yx)) * sign(e);
    }
    return e * 0.5 + 0.5; // map from [-1,1] to [0,1]
}

vec3 GetNormalFromMap(vec3 sampledNormal) {
    vec3 tangentNormal = sampledNormal * 2.0 - 1.0;

    vec3 Q1 = dFdx(iWorldPos);
    vec3 Q2 = dFdy(iWorldPos);
    vec2 st1 = dFdx(iUV);
    vec2 st2 = dFdy(iUV);

    vec3 N = normalize(iNormal);
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
// cosTheta is dot product between normal and view dir
vec3 FresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness) {
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

float ShadowCalculation(vec3 L, vec3 N) {
    vec3 projCoords;
    uint shadowMapIndex = 0;

    for (uint i = 0; i < 3; i++) {
        vec4 lightSpace = lightMatrices[i] * vec4(iWorldPos, 1.0);
        vec3 proj;
        proj = lightSpace.xyz / lightSpace.w;
        proj = proj * 0.5 + 0.5;

        if (proj.z > 1.0 || proj.x < 0.0 || proj.x > 1.0 || proj.y < 0.0 || proj.y > 1.0) {
            continue;
        } else {
            projCoords = proj;
            shadowMapIndex = i;
            break;
        }
    }

    float closestDepth = texture(sampler2D(shadowMaps[shadowMapIndex], shadowSampler), projCoords.xy).r;

    float currentDepth = projCoords.z;

    currentDepth = (1.0 - currentDepth) * 2;

    float bias = max(0.05 * (1.0 - dot(N, L)), 0.005);

    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadowMaps[shadowMapIndex], 0);

    for (int x = -1; x <= 1; ++x)
    {
        for (int y = -1; y <= 1; ++y)
        {
            float pcfDepth = texture(sampler2D(shadowMaps[shadowMapIndex], shadowSampler), projCoords.xy + vec2(x, y) * texelSize).r;
            shadow += currentDepth + bias < pcfDepth ? 0.0 : 1.0;
        }
    }
    shadow /= 9.0;

    return shadow;
}
