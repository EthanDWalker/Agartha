#pragma once
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <vulkan/vulkan.h>

struct GpuObject {
  VkDeviceAddress vertex_buffer;
  VkDeviceAddress index_buffer;
  VkDeviceAddress instance_buffer;
  uint32_t aabb_buffer_index;
};

struct Vertex {
  glm::vec3 position;
  float uv_x;
  glm::vec3 normal;
  float uv_y;
  glm::vec4 color;
};

struct PointLight {
  glm::vec4 color; // w = intensity
  glm::vec3 position;
};

struct DirectionalLight {
  glm::vec3 direction;
};

struct Material {
  int32_t albedo{-1};
  int32_t metal_roughness{-1};
  int32_t emissive{-1};
  int32_t normal{-1};
  int32_t ambient_occlusion{-1};
};

struct AABB {
  glm::vec3 min;
  float padding;
  glm::vec3 max;
  float padding_1;
};
