#pragma once
#include "Backend/buffer.h"
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

struct Vertex {
  glm::vec3 position;
  float uv_x;
  glm::vec3 normal;
  float uv_y;
};

struct PointLight {
  glm::vec4 color; // w = intensity
  glm::vec3 position;
  float padding;
};

struct DirectionalLight {
  glm::vec4 direction;
};

struct Material {
  int32_t albedo{-1};
  int32_t metal_roughness{-1};
  int32_t emissive{-1};
  int32_t normal{-1};
  int32_t ambient_occlusion{-1};
};

struct Mesh {
  AllocatedBuffer vertex_buffer;
  uint32_t first_index;
  uint32_t index_count;
};

struct Instance {
  glm::mat4 matrix;
  glm::vec3 color;
  uint32_t object_index;
};
