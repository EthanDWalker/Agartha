#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_samplerless_texture_functions : require
#include "common.glsl"

layout(location = 0) in vec3 iNormal;
layout(location = 1) in vec3 iWorldPos;
layout(location = 2) in vec2 iUV;
layout(location = 3) flat in uint iObjectIndex;

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

layout(set = 7, binding = 0, rgba16) readonly uniform image3D radianceImageMips[];

layout(set = 7, binding = 1) uniform SvoDataUbo {
    SvoData svoData;
};

layout(set = 7, binding = 2) uniform sampler3D radianceImage;

const float PI = 3.14159265359;

vec3 GetNormalFromMap(vec3 sampledNormal);

float DistributionGGX(vec3 N, vec3 H, float roughness);

float GeometrySchlickGGX(float NdotV, float roughness);
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness);

vec3 FresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness);

float ShadowCalculation(vec3 L, vec3 N, DirectionalLight directionalLight);

vec2 OctEncodeNormal(vec3 n);

vec3 CalculateIndirectLight(vec3 N);

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

        vec3 radiance = directionalLight.color * directionalLight.intensity;

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

        float shadow = ShadowCalculation(L, N, directionalLight);

        Lo += shadow * (kD * albedo / PI + specular) * radiance * NdotL;
    }

    {
        vec3 radiance = CalculateIndirectLight(N);

        vec3 F = FresnelSchlickRoughness(max(dot(N, V), 0.0), F0, roughness);

        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;

        kD *= 1.0 - metallic;

        Lo += (kD * albedo) * radiance;
    }

    vec3 color = Lo;

    oColor = vec4(color, 1.0);

    vec2 packedNormal = OctEncodeNormal(N);

    oMrNormal = vec4(metallic, roughness, packedNormal.x, packedNormal.y);
}

mat3 GetTBN() {
    vec3 Q1 = dFdx(iWorldPos);
    vec3 Q2 = dFdy(iWorldPos);
    vec2 st1 = dFdx(iUV);
    vec2 st2 = dFdy(iUV);

    vec3 N = normalize(iNormal);
    vec3 T = normalize(Q1 * st2.t - Q2 * st1.t);
    vec3 B = -normalize(cross(N, T));
    mat3 TBN = mat3(T, B, N);

    return TBN;
}

vec3 GetNormalFromMap(vec3 sampledNormal) {
    vec3 tangentNormal = sampledNormal * 2.0 - 1.0;

    mat3 TBN = GetTBN();

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

float ShadowCalculation(vec3 L, vec3 N, DirectionalLight directionalLight) {
    vec3 projCoords;
    uint shadowMapIndex = 0;

    for (uint i = directionalLight.cascade_index; i < directionalLight.cascade_index + 3; i++) {
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

    if (projCoords.z > 1.0 || projCoords.x < 0.0 || projCoords.x > 1.0 || projCoords.y < 0.0 || projCoords.y > 1.0)
        return 1.0;

    float closestDepth = texture(sampler2D(shadowMaps[shadowMapIndex], shadowSampler), projCoords.xy).r;

    float currentDepth = projCoords.z;

    currentDepth = (1.0 - currentDepth) * 2;

    float bias = max(0.05 * (1.0 - dot(N, L)), 0.005);

    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadowMaps[shadowMapIndex], 0);

    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            float pcfDepth = texture(sampler2D(shadowMaps[shadowMapIndex], shadowSampler),
                    projCoords.xy + vec2(x, y) * texelSize).r;
            shadow += currentDepth + bias < pcfDepth ? 0.0 : 1.0;
        }
    }
    shadow /= 9.0;

    return shadow;
}

vec3 SafeNormalize(vec3 v) {
    v = normalize(v);
    return any(isinf(v)) || any(isnan(v)) ? vec3(0.0, 1.0, 0.0) : v;
}

vec3 CalculateIndirectLight(vec3 N) {
    const vec3 coneDirections[5] = vec3[5](
            vec3(0.0, 0.0, 1.0),
            vec3(0.0, 0.707106781, 0.707106781),
            vec3(0.0, -0.707106781, 0.707106781),
            vec3(0.707106781, 0.0, 0.707106781),
            vec3(-0.707106781, 0.0, 0.707106781)
        );
    const float coneWeights[5] = float[5](
            0.28, 0.18, 0.18, 0.18, 0.18
        );

    const float coneHalfAngle = radians(25.0);

    const float invSqrt2 = 1.0 / sqrt(2.0);
    const vec3 normalOffset = N * (1.0 + 4.0 * invSqrt2) * svoData.voxelSize;

    const mat3 TBN = GetTBN();

    vec3 mini = -svoData.leftBound;
    vec3 maxi = svoData.leftBound;
    vec3 extent = svoData.leftBound * 2.0;
    vec3 invExtent = 1.0 / extent;

    const vec3 coneOrigin = iWorldPos + maxi + normalOffset;

    vec3 indirectDiffuse = vec3(0.0);
    for (uint i = 0; i < 5; ++i) {
        const vec3 coneDirection = normalize(TBN * coneDirections[i]);

        vec3 indirectColor = vec3(0.0);
        float occlusion = 0.0;

        float marchedDistance = 0.2;
        const float MAX_DIST = 50.0;

        while (occlusion < 1.0 && marchedDistance < MAX_DIST) {
            vec3 svoPos = coneOrigin + marchedDistance * coneDirection;

            if (any(lessThanEqual(svoPos, vec3(0))) || any(greaterThanEqual(svoPos, extent))) {
                break;
            }

            marchedDistance = marchedDistance + 2.0 * tan(coneHalfAngle) * marchedDistance;
            float level = log2(marchedDistance);

            vec4 voxel = textureLod(radianceImage, svoPos * invExtent, level);
            indirectColor = indirectColor + voxel.rgb * (1.0 - occlusion);
            occlusion = occlusion + (1.0 - occlusion) * voxel.a;
        }

        indirectDiffuse += coneWeights[i] * indirectColor.rgb;
    }

    return indirectDiffuse;
}
