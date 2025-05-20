#version 450

struct PointLight {
    vec4 color;
    vec3 position;
    vec4 ambient;
    vec4 diffuse;
    vec4 specular;
};

layout(location = 0) in vec3 vertColor;
layout(location = 1) in vec3 vertNormal;
layout(location = 2) in vec3 fragPos;
layout(location = 3) in vec2 uv;

layout(location = 0) out vec4 outColor;

layout(std140, binding = 0) uniform LightUBO {
    PointLight light;
};

layout(binding = 1) uniform sampler textureSampler;

layout(binding = 2) uniform texture2D pbrTexture[];

layout(push_constant) uniform constants
{
    mat4 worldMatrix;
    vec3 viewPos;
} PushConstants;

void main() {
    vec3 ambient = light.ambient.xyz * vec3(texture(sampler2D(pbrTexture[0], textureSampler), uv));

    vec3 norm = normalize(vertNormal);
    vec3 lightDir = normalize(light.position - fragPos);
    float diff = max(dot(norm, lightDir), 0.0);

    vec3 diffuse = light.diffuse.xyz * diff * vec3(texture(sampler2D(pbrTexture[0], textureSampler), uv));

    vec3 viewDir = normalize(PushConstants.viewPos - fragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);

    vec3 specular = light.specular.xyz * spec * vec3(texture(sampler2D(pbrTexture[1], textureSampler), uv));

    vec3 result = (ambient + diffuse + specular) * light.color.w + vec3(texture(sampler2D(pbrTexture[2], textureSampler), uv));

    outColor = vec4(result, 1.0);
}
