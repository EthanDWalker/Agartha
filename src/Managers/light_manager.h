#pragma once
#include "Backend/allocated_image.h"
#include "Backend/buffer.h"
#include "Backend/context.h"
#include "Backend/descriptors.h"
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

const std::array<float, 3> CASCADES = {10.0f, 25.0f, 100.0f};

const uint32_t MAX_POINT_LIGHTS = 10;
const uint32_t MAX_DIRECTIONAL_LIGHTS = 1;
const uint32_t MAX_LIGHT_MATRICES = MAX_DIRECTIONAL_LIGHTS * CASCADES.size();

const float DIRECTIONAL_LIGHT_DISTANCE = 1000.0f;
const float DIRECTIONAL_LIGHT_FAR_PLANE = 1100.0f;
const float DIRECTIONAL_LIGHT_NEAR_PLANE = 300.1f;

const VkExtent3D SHADOW_IMAGE_EXTENT = {1024, 1024, 1};

struct DirectionalLight {
  glm::vec3 color;
  float intensity;
  glm::vec3 direction;
  uint32_t cascade_index;
};

struct PointLight {
  glm::vec3 color;
  float intensity;
  glm::vec3 position;
  uint32_t shadow_map_index;
};

struct LightManager {
  AllocatedBuffer matrix_buffer;
  AllocatedBuffer directional_light_buffer;
  AllocatedBuffer point_light_buffer;

  std::vector<AllocatedImage> shadow_images;
  std::vector<glm::vec3> directional_lights;

  VkDescriptorSet light_descriptor_set;
  VkDescriptorSetLayout light_descriptor_layout;

  VkDescriptorSet shadow_descriptor_set;
  VkDescriptorSetLayout shadow_descriptor_layout;

  VkSampler shadow_sampler;

  uint32_t point_light_index;
  uint32_t directional_light_index;
  uint32_t matrix_index;

  void Init(VulkanContext &context, DescriptorBuilder &descriptor_builder);

  uint32_t AddDirectionalLight(VulkanContext &context, glm::vec3 color,
                               glm::vec3 direction, float intensity,
                               ImmediateSubmit &immediate_submit);

  uint32_t AddPointLight(VulkanContext &context, glm::vec3 color,
                         glm::vec3 position, float intensity,
                         ImmediateSubmit &immediate_submit);

  void UpdateMatrices(VulkanContext &context, ImmediateSubmit &immediate_submit,
                      glm::vec3 camera_position);

  void Destroy(VulkanContext &context);
};
