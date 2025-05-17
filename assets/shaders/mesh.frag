#version 450

struct PointLight {
  vec4 color;
  vec3 position;
};

struct Material {
  vec3 ambient;
  float shininess;
  vec3 diffuse;
  float padding;
  vec3 specular;
};

layout(location = 0) in vec3 vertColor;
layout(location = 1) in vec3 vertNormal;
layout(location = 2) in vec3 fragPos;
layout(location = 3) in vec2 uv;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D myTexture;

layout(std140, binding = 1) uniform LightUBO {
  PointLight light;
} lightData;

layout(std140, binding = 2) uniform MaterialUBO {
  Material arr[1];
} materials;

layout( push_constant ) uniform constants
{
  mat4 worldMatrix;
  vec3 viewPos;
  uint material_index;
} PushConstants;

void main() {
    Material mat = materials.arr[PushConstants.material_index];

    vec3 ambient = lightData.light.color.xyz * mat.ambient;

    vec3 norm = normalize(vertNormal);
    vec3 lightDir = normalize(lightData.light.position - fragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = (diff * mat.diffuse) * lightData.light.color.xyz * lightData.light.color.w;

    vec3 viewDir = normalize(PushConstants.viewPos - fragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), mat.shininess);
    vec3 specular = (mat.specular * spec) * lightData.light.color.xyz * lightData.light.color.w;

    vec3 result = (ambient + diffuse + specular);

    outColor = texture(myTexture, uv) * vec4(result, 1.0);
}
