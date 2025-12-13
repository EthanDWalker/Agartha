#pragma once
#include "Backend/buffer.h"
#include "Backend/descriptors.h"
#include <GLFW/glfw3.h>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

struct Frustum {
  glm::vec4 top;
  glm::vec4 bottom;
  glm::vec4 right;
  glm::vec4 left;
  glm::vec4 far;
  glm::vec4 near;
};

struct CameraBuffer {
  glm::mat4 view_matrix;
  glm::mat4 projection_matrix;
  glm::mat4 inv_view;
  glm::mat4 inv_proj;
  glm::vec3 view_pos;
  float padding;
  Frustum frustum;
  float near;
  float far;
};

struct Camera {
  CameraBuffer buffer_data;
  AllocatedBuffer buffer;

  glm::vec3 velocity;
  glm::vec3 position;
  VkDescriptorSet descriptor_set;
  VkDescriptorSetLayout descriptor_layout;
  float aspect_ratio;

  float pitch{0.0f};
  float yaw{0.0f};

  void Create(DescriptorBuilder &descriptor_builder, glm::vec2 window_size);

  void Resize(glm::vec2 window_size);

  void Update(GLFWwindow *window, float delta_time);

  void Destroy();
};
