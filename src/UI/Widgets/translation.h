#pragma once

#include "Backend/buffer.h"
#include "Backend/descriptors.h"
#include "Backend/pipeline.h"
#include "camera.h"
#include <cstdint>
#include <glm/gtc/matrix_transform.hpp>

struct TranslationWidget {
  enum TranslationDirections : uint8_t {
    X = 0,
    Y = 1,
    Z = 2,
    COUNT = 3,
  };

  enum TranslationMode : uint8_t {
    MOVE,
    ROTATE,
    SCALE,
  };

  static constexpr size_t TRANSLATION_DIRECTION_COUNT =
      static_cast<size_t>(TranslationDirections::COUNT);

  const glm::vec3 DIRECTION_VECTORS[TRANSLATION_DIRECTION_COUNT] = {
      glm::vec3(1.0f, 0.0f, 0.0f),
      glm::vec3(0.0f, 1.0f, 0.0f),
      glm::vec3(0.0f, 0.0f, 1.0f),
  };

  const glm::vec3 DIRECTION_PLANES[TRANSLATION_DIRECTION_COUNT] = {
      glm::vec3(0.0f, 1.0f, 0.0f),
      glm::vec3(1.0f, 0.0f, 0.0f),
      glm::vec3(0.0f, 1.0f, 0.0f),
  };

  const glm::vec4 DIRECTION_COLORS[TRANSLATION_DIRECTION_COUNT] = {
      glm::vec4(1.0f, 0.0f, 0.0f, 1.0f),
      glm::vec4(0.0f, 1.0f, 0.0f, 1.0f),
      glm::vec4(0.0f, 0.0f, 1.0f, 1.0f),
  };

  const glm::mat4 DIRECTION_INSTANCES[TRANSLATION_DIRECTION_COUNT] = {
      glm::translate(glm::mat4(1.0f), DIRECTION_VECTORS[0] * 3.0f),
      glm::rotate(glm::translate(glm::mat4(1.0f), DIRECTION_VECTORS[1] * 3.0f),
                  glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
      glm::rotate(glm::translate(glm::mat4(1.0f), DIRECTION_VECTORS[2] * 3.0f),
                  glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f)),
  };

  glm::mat4 matrix;

  AllocatedBuffer instance_buffer;
  AllocatedBuffer vertex_buffer;
  AllocatedBuffer index_buffer;
  AllocatedBuffer color_buffer;

  glm::vec3 bounds_min;
  glm::vec3 bounds_max;

  Pipeline draw_pipeline;

  VkDescriptorSet descriptor_set;
  VkDescriptorSetLayout descriptor_layout;

  TranslationDirections selected_direction{TranslationDirections::COUNT};

  TranslationMode selected_mode{TranslationMode::MOVE};

  void Create(VulkanContext &vulkan_context, ImmediateSubmit &immediate_submit,
              DescriptorBuilder &descriptor_builder, Camera &camera,
              VkFormat draw_format);

  void Update(GLFWwindow *window, Camera &camera);

  void Draw(VkCommandBuffer cmd, Camera &camera);

  void Destroy(VulkanContext &vulkan_context);
};
