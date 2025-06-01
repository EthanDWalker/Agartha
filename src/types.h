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

