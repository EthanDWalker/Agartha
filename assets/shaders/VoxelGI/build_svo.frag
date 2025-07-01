#version 450
#extension GL_EXT_buffer_reference : require
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_shader_atomic_float2 : require
#include "../common.glsl"

layout(set = 0, binding = 0) buffer SvoBuffer {
    SvoNodeBuffer svo[];
};

layout(set = 0, binding = 1) uniform SvoDataUbo {
    SvoData svoData;
};

layout(set = 2, binding = 0) readonly buffer ObjectBuffer {
    Object objects[];
};

layout(set = 3, binding = 0) uniform texture2D textures[];

layout(set = 3, binding = 1) uniform sampler textureSampler;

layout(set = 4, binding = 0) readonly buffer PointLightBuffer {
    PointLight pointLight;
};

layout(set = 4, binding = 1) readonly buffer DirectionalLightBuffer {
    DirectionalLight directionalLight;
};

layout(set = 5, binding = 0) uniform texture2D shadowMaps[];

layout(set = 5, binding = 1) readonly buffer LightMatrixBuffer {
    mat4 lightMatrices[];
};

layout(set = 5, binding = 2) uniform sampler shadowSampler;

layout(set = 6, binding = 0) uniform CameraUBO {
    Camera camera;
};

layout(location = 0) in vec3 iWorldPos;
layout(location = 1) in vec3 iNormal;
layout(location = 2) in vec2 iUv;
layout(location = 3) flat in uint iObjectIndex;

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

    float depth = texture(sampler2D(shadowMaps[shadowMapIndex], shadowSampler), projCoords.xy).r;
    float shadow = currentDepth + bias < depth ? 0.0 : 1.0;

    return shadow;
}

vec3 FresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness) {
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

const float PI = 3.14159265359;

void main() {
    Material mat = objects[iObjectIndex].material;

    float lod = 5.0;
    vec4 albedoAo = textureLod(sampler2D(textures[mat.albedoAo], textureSampler), iUv, lod);
    vec4 mrNormal = textureLod(sampler2D(textures[mat.mrNormal], textureSampler), iUv, lod);
    float metallic = clamp(mrNormal.x, 0.0, 1.0);
    float roughness = clamp(mrNormal.y, 0.0, 1.0);

    vec3 albedo = albedoAo.rgb;
    float ao = albedoAo.a;

    vec3 N = normalize(iNormal);
    vec3 V = normalize(camera.viewPos - iWorldPos);

    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metallic);

    vec3 Lo = vec3(0.0);

    {
        vec3 L = normalize(-directionalLight.direction);
        vec3 H = normalize(V + L);

        vec3 radiance = directionalLight.color * directionalLight.intensity;

        vec3 F = FresnelSchlickRoughness(max(dot(N, V), 0.0), F0, roughness);

        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;

        kD *= 1.0 - metallic;

        float NdotL = max(dot(N, L), 0.0);

        float shadow = ShadowCalculation(L, N, directionalLight);

        Lo += shadow * (kD * albedo) * radiance * NdotL;
    }

    vec3 color = Lo;

    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0 / 2.2));

    vec3 min = -svoData.leftBound;
    vec3 max = svoData.leftBound;
    vec3 extent = svoData.leftBound * 2.0;

    if (iWorldPos.x <= min.x || iWorldPos.x >= max.x ||
            iWorldPos.y <= min.y || iWorldPos.y >= max.y ||
            iWorldPos.z <= min.z || iWorldPos.z >= max.z) discard;

    vec3 svoPos = iWorldPos + max;

    for (uint i = 0; i < svoData.depth; ++i) {
        uint voxelsPerDimension = 1u << (i + 1);

        vec3 normalizedPos = svoPos / extent.x;
        vec3 levelPosition = floor(normalizedPos * float(voxelsPerDimension));

        uint levelIndex = uint(levelPosition.x +
                    levelPosition.y * voxelsPerDimension +
                    levelPosition.z * voxelsPerDimension * voxelsPerDimension);

        uint packedColor = packUnorm4x8(vec4(color, 0.0));
        packedColor |= 1;
        atomicMax(svo[i].nodes[levelIndex].color, packedColor);
        atomicMax(svo[i].nodes[levelIndex].normal, packUnorm2x16(OctEncodeNormal(N)));
    }
}
