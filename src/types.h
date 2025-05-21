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

struct SkyboxPushConstantData {
  glm::mat4 proj_matrix;
  glm::mat4 view_matrix;
  VkDeviceAddress vertex_buffer;
};

struct PushConstantData {
  glm::mat4 world_matrix;
  glm::vec3 view_pos;
  float padding;
  VkDeviceAddress vertex_buffer;
};

struct PointLight {
  glm::vec4 color; // w = intensity
  glm::vec3 position;
};
