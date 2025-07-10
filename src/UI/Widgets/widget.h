#pragma once

#include "Backend/buffer.h"
#include "camera.h"
#include <glm/vec3.hpp>

struct Widget {
  AllocatedBuffer index_buffer;
  AllocatedBuffer vertex_buffer;

  glm::mat4 matrix;

  glm::vec3 bound_min;
  glm::vec3 bound_max;

  glm::vec2 drag_position;

  VkDeviceAddress vertex_address;

  bool selected;

  void Create(VulkanContext &vulkan_context, glm::mat4 matrix,
              std::string model);

  void Update(GLFWwindow *window, Camera &camera);

  void Destroy(VulkanContext &vulkan_context);
};
