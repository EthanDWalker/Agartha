#version 450

struct PointLight {
    vec4 color;
    vec3 position;
    vec4 ambient;
    vec4 diffuse;
    vec4 specular;
};

layout(location = 0) in vec3 iColor;
layout(location = 1) in vec3 iNormal;
layout(location = 2) in vec3 iWorldPos;
layout(location = 3) in vec2 iUV;

layout(location = 0) out vec4 oColor;

layout(std140, binding = 0) uniform LightUBO {
    PointLight light;
};

layout(binding = 1) uniform sampler textureSampler;

layout(binding = 2) uniform textureCube irradianceMap;

layout(binding = 3) uniform texture2D pbrTexture[];

layout(push_constant) uniform constants
{
    mat4 worldMatrix;
    vec3 viewPos;
} PushConstants;

const float PI = 3.14159265359;

vec3 getNormalFromMap();

float DistributionGGX(vec3 N, vec3 H, float roughness);

float GeometrySchlickGGX(float NdotV, float roughness);
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness);

vec3 FresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness);

void main() {
    vec3 albedo = texture(sampler2D(pbrTexture[0], textureSampler), iUV).rgb;
    float metallic = texture(sampler2D(pbrTexture[1], textureSampler), iUV).b;
    float roughness = texture(sampler2D(pbrTexture[1], textureSampler), iUV).g;
    vec3 emmisive = texture(sampler2D(pbrTexture[2], textureSampler), iUV).rgb;
    float ao = texture(sampler2D(pbrTexture[3], textureSampler), iUV).r;

    vec3 N = getNormalFromMap();

    vec3 V = normalize(PushConstants.viewPos - iWorldPos);

    vec3 Lo = vec3(0.0);
    // loop through lights but i only have 1
    vec3 L = normalize(light.position - iWorldPos);
    vec3 H = normalize(V + L);
    float distance = length(light.position - iWorldPos);
    float attenuation = 1.0 / (distance * distance);
    vec3 radiance = light.color.xyz * attenuation * light.color.w;

    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metallic);
    vec3 F = FresnelSchlickRoughness(max(dot(H, V), 0.0), F0, roughness);

    float NDF = DistributionGGX(N, H, roughness);
    float G = GeometrySmith(N, V, L, roughness);

    vec3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
    vec3 specular = numerator / denominator;

    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    vec3 irradience = texture(samplerCube(irradianceMap, textureSampler), N).rgb;
    vec3 diffuse = irradience * albedo;

    kD *= 1.0 - metallic;

    float NdotL = max(dot(N, L), 0.0);

    Lo += (kD * albedo / PI + specular) * radiance * NdotL;
    // end loop

    vec3 ambient = (kD * diffuse) * ao;
    vec3 color = ambient + Lo + emmisive;

    // HDR
    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0 / 2.2));
    oColor = vec4(color, 1.0);
}

vec3 getNormalFromMap()
{
    vec3 tangentNormal = texture(sampler2D(pbrTexture[4], textureSampler), iUV).xyz * 2.0 - 1.0;

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
float DistributionGGX(vec3 N, vec3 H, float roughness)
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
float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

    float num = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return num / denom;
}
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
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
vec3 FresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness)
{
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}
