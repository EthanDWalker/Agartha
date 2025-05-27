#pragma once
#include "Backend/buffer.h"
#include "Backend/context.h"
#include "Backend/descriptors.h"
#include <GLFW/glfw3.h>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

struct CameraUboData {
  glm::mat4 view_matrix;
  glm::mat4 projection_matrix;
  glm::vec3 view_pos;
  float padding;
};

struct Camera {
public:
  glm::vec3 velocity;
  glm::vec3 position;

  float pitch{0.0f};
  float yaw{0.0f};

  AllocatedBuffer ubo;
  VkDescriptorSet descriptor_set;
  VkDescriptorSetLayout descriptor_layout;

  void Create(VulkanContext &context, DescriptorBuilder &descriptor_builder);

  void Update(VulkanContext &context, ImmediateSubmit &immediate_submit,
              GLFWwindow *window, float delta_time);

  void Destroy(VulkanContext &context);
};
