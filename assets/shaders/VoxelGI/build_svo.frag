#version 450
#extension GL_EXT_buffer_reference : require
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_samplerless_texture_functions : require
#include "../common.glsl"

layout(set = 0, binding = 0) buffer SVOLevel1Buffer {
    SvoNode l1Buffer[];
};

layout(set = 0, binding = 1) buffer SVOLevel2Buffer {
    SvoNode l2Buffer[];
};

layout(set = 0, binding = 2) buffer SVOLevel3Buffer {
    SvoNode l3Buffer[];
};

layout(set = 0, binding = 3) buffer SVOLevel4Buffer {
    SvoNode l4Buffer[];
};

layout(set = 0, binding = 4) buffer SVOLevel5Buffer {
    SvoNode l5Buffer[];
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

layout(location = 0) in GS_OUT {
    vec3 position;
    vec3 worldPos;
    uint objectIndex;
    vec3 normal;
    vec2 uv;
};

float ShadowCalculation(vec3 L, vec3 N, DirectionalLight directionalLight) {
    vec3 projCoords;
    uint shadowMapIndex = 0;

    for (uint i = directionalLight.cascade_index; i < directionalLight.cascade_index + 3; i++) {
        vec4 lightSpace = lightMatrices[i] * vec4(worldPos, 1.0);
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

SvoNode PackData(SvoNode currentData, vec3 normal, vec3 color) {
    SvoNode node;
    node.visible = 1;
    if ((currentData.visible & 0x1) == 0) {
        node.color = color;
        return node;
    }
    node.color = mix(color, currentData.color, 0.5);
    return node;
}

void main() {
    Material material = objects[objectIndex].material;

    vec3 color = texture(sampler2D(textures[material.albedoAo], textureSampler), uv).rgb;

    vec3 L = normalize(-directionalLight.direction);
    color *= ShadowCalculation(L, normalize(normal), directionalLight);

    vec3 l1Position = position;
    vec3 l1RoundedPosition = floor(l1Position * 2.0);
    uint l1Index = uint(l1RoundedPosition.x * 1) + uint(l1RoundedPosition.y * 2) + uint(l1RoundedPosition.z * 4);
    l1Buffer[l1Index] = PackData(l1Buffer[l1Index], normal, color);

    vec3 l2Position = (l1Position - (vec3(0.5) * l1RoundedPosition)) * 2.0;
    vec3 l2RoundedPosition = floor(l2Position * 2.0);
    uint l2Index = uint(l2RoundedPosition.x * 1) + uint(l2RoundedPosition.y * 2) + uint(l2RoundedPosition.z * 4);
    l2Index += l1Index * 8;
    l2Buffer[l2Index] = PackData(l2Buffer[l2Index], normal, color);

    vec3 l3Position = (l2Position - (vec3(0.5) * l2RoundedPosition)) * 2.0;
    vec3 l3RoundedPosition = floor(l3Position * 2.0);
    uint l3Index = uint(l3RoundedPosition.x * 1) + uint(l3RoundedPosition.y * 2) + uint(l3RoundedPosition.z * 4);
    l3Index += l2Index * 8;
    l3Buffer[l3Index] = PackData(l3Buffer[l3Index], normal, color);

    vec3 l4Position = (l3Position - (vec3(0.5) * l3RoundedPosition)) * 2.0;
    vec3 l4RoundedPosition = floor(l4Position * 2.0);
    uint l4Index = uint(l4RoundedPosition.x * 1) + uint(l4RoundedPosition.y * 2) + uint(l4RoundedPosition.z * 4);
    l4Index += l3Index * 8;
    l4Buffer[l4Index] = PackData(l4Buffer[l4Index], normal, color);

    vec3 l5Position = (l4Position - (vec3(0.5) * l4RoundedPosition)) * 2.0;
    vec3 l5RoundedPosition = floor(l5Position * 2.0);
    uint l5Index = uint(l5RoundedPosition.x * 1) + uint(l5RoundedPosition.y * 2) + uint(l5RoundedPosition.z * 4);
    l5Index += l4Index * 8;
    l5Buffer[l5Index] = PackData(l5Buffer[l5Index], normal, color);
}
