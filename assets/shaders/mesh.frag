#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_samplerless_texture_functions : require
#include "common.glsl"

layout(location = 0) in vec3 iNormal;
layout(location = 1) in vec3 iWorldPos;
layout(location = 2) in vec2 iUV;
layout(location = 3) in vec4 iLightSpace;
layout(location = 4) flat in uint iObjectIndex;

layout(location = 0) out vec4 oColor;

layout(std140, set = 0, binding = 0) uniform PointLightUBO {
    PointLight pointLight;
};

layout(std140, set = 0, binding = 1) uniform DirectionalLightUBO {
    DirectionalLight directionalLight;
};

layout(set = 0, binding = 2) uniform sampler textureSampler;

layout(set = 0, binding = 4) uniform sampler2D shadowMap;

layout(set = 1, binding = 0) uniform texture2D textures[];

layout(set = 2, binding = 0) uniform CameraUBO {
    Camera camera;
};

layout(set = 3, binding = 0) readonly buffer ObjectBuffer {
    Object objects[];
};

const float PI = 3.14159265359;

vec3 getNormalFromMap();

float DistributionGGX(vec3 N, vec3 H, float roughness);

float GeometrySchlickGGX(float NdotV, float roughness);
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness);

vec3 FresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness);

float ShadowCalculation(vec3 L, vec3 N);

vec3 ComputeFog(
    vec3 cameraPos,
    vec3 fragPos,
    vec3 lightPos,
    vec4 lightColor,
    float fogDensity
) {
    vec3 viewDir = fragPos - cameraPos;
    float viewLength = length(viewDir);
    vec3 viewDirNorm = normalize(viewDir);

    vec3 lightToCamera = cameraPos - lightPos;
    float lightToCameraLength = length(lightToCamera);
    vec3 lightToCameraNorm = normalize(lightToCamera);

    float h = length(cross(viewDirNorm, lightToCamera));

    float a = dot(lightToCamera, viewDirNorm);
    float b = a + viewLength;

    float scattering = atan(b / h) - atan(a / h);
    scattering /= h;

    return lightColor.xyz * lightColor.w * scattering * fogDensity;
}

void main() {
    Material mat = objects[iObjectIndex].material;

    vec3 albedo = texture(sampler2D(textures[mat.albedo], textureSampler), iUV).rgb;
    vec3 mr = texture(sampler2D(textures[mat.metal_roughness], textureSampler), iUV).rgb;
    float metallic = mr.b;
    float roughness = mr.g;
    vec3 emisive = texture(sampler2D(textures[mat.emissive], textureSampler), iUV).rgb;
    float ao = texture(sampler2D(textures[mat.ambient_occlusion], textureSampler), iUV).r;

    vec3 N = getNormalFromMap();
    vec3 V = normalize(camera.viewPos - iWorldPos);
    vec3 R = reflect(-V, N);

    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metallic);

    vec3 Lo = vec3(0.0);

    // loop through lights but i only have 1
    {
        vec3 L = normalize(pointLight.position - iWorldPos);
        vec3 H = normalize(V + L);

        float distance = length(pointLight.position - iWorldPos);
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
        Lo += ComputeFog(camera.viewPos, iWorldPos, pointLight.position, pointLight.color, 0.001);
    }

    // directional light
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

        float shadow = ShadowCalculation(L, N) + .05;

        Lo += shadow * (kD * albedo / PI + specular) * radiance * NdotL;
    }
    // end loop

    vec3 F = FresnelSchlickRoughness(max(dot(N, V), 0.0), F0, roughness);

    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metallic;

    vec3 color = Lo + emisive + (F + albedo) * kD * ao;

    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0 / 2.2));

    oColor = vec4(color, 1.0);
}

vec3 getNormalFromMap()
{
    Material mat = objects[iObjectIndex].material;
    vec3 tangentNormal = texture(sampler2D(textures[mat.normal], textureSampler), iUV).xyz * 2.0 - 1.0;

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
float DistributionGGX(vec3
    N, vec3
    H, float
    roughness)
{
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

float ShadowCalculation(vec3 L, vec3 N) {
    vec3 projCoords = iLightSpace.xyz / iLightSpace.w;

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
