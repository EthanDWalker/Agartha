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

struct PointLight {
  glm::vec4 color; // w = intensity
  glm::vec3 position;
};

/*
struct Material {
  uint32_t albedo;
  uint32_t metal_roughness;
  uint32_t emmissive;
  uint32_t normal;
  uint32_t ambient_occlusion;
};
*/
