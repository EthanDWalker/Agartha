#version 450

layout(location = 0) out vec4 FragColor;

layout(location = 0) in vec3 localPos;

layout(set = 0, binding = 0) uniform samplerCube environmentMap;

const vec2 invAtan = vec2(0.1591, 0.3183);

void main()
{
    vec3 envColor = texture(environmentMap, localPos).rgb;

    envColor = envColor / (envColor + vec3(1.0));
    envColor = pow(envColor, vec3(1.0 / 2.2));

    FragColor = vec4(envColor, 1.0);
}
