#pragma once
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <vulkan/vulkan.h>

struct Vertex {
  glm::vec3 position;
  float uv_x;
  glm::vec3 normal;
  float uv_y;
  glm::vec4 color;
};

struct PushConstantData {
  glm::mat4 world_matrix;
  glm::vec3 view_pos;
  uint32_t material_index;
  VkDeviceAddress vertex_buffer;
};

struct PointLight {
  glm::vec4 color; // w = intensity
  glm::vec3 position;
};

struct Material {
  glm::vec3 ambient;
  float shininess;
  glm::vec3 diffuse;
  float padding;
  glm::vec3 specular;
};
