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

struct Material {
  int32_t albedo_ao{-1};
  int32_t mr_normal{-1};
};

struct Mesh {
  AllocatedBuffer vertex_buffer;
  uint32_t first_index;
  uint32_t index_count;
};
