#include "light_manager.h"
#include "Backend/allocated_image.h"
#include "Backend/buffer.h"
#include "Backend/context.h"
#include "Backend/immediate_submit.h"
#include <cstddef>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/transform.hpp>

void LightManager::Init(VulkanContext &context,
                        DescriptorBuilder &descriptor_builder) {
  CreateBuffer(context, sizeof(glm::mat4) * MAX_LIGHT_MATRICES,
               VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                   VK_BUFFER_USAGE_TRANSFER_DST_BIT,
               VMA_MEMORY_USAGE_GPU_ONLY, matrix_buffer);

  CreateBuffer(context, sizeof(DirectionalLight) * MAX_DIRECTIONAL_LIGHTS,
               VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                   VK_BUFFER_USAGE_TRANSFER_DST_BIT,
               VMA_MEMORY_USAGE_GPU_ONLY, directional_light_buffer);
  CreateBuffer(context, sizeof(PointLight) * MAX_POINT_LIGHTS,
               VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                   VK_BUFFER_USAGE_TRANSFER_DST_BIT,
               VMA_MEMORY_USAGE_GPU_ONLY, point_light_buffer);

  descriptor_builder.Reset();
  descriptor_builder.BindStorageBuffer(0, point_light_buffer.buffer);
  descriptor_builder.BindStorageBuffer(1, directional_light_buffer.buffer);
  descriptor_builder.Build(context, VK_SHADER_STAGE_ALL, light_descriptor_set,
                           light_descriptor_layout);

  VkSamplerCreateInfo shadow_sampler_ci{};
  shadow_sampler_ci.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  shadow_sampler_ci.magFilter = VK_FILTER_LINEAR;
  shadow_sampler_ci.minFilter = VK_FILTER_LINEAR;
  shadow_sampler_ci.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  shadow_sampler_ci.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  shadow_sampler_ci.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  shadow_sampler_ci.compareEnable = VK_TRUE;
  shadow_sampler_ci.compareOp = VK_COMPARE_OP_GREATER_OR_EQUAL;
  shadow_sampler_ci.maxLod = 1.0f;

  vkCreateSampler(context.device, &shadow_sampler_ci, nullptr, &shadow_sampler);

  shadow_images.resize(MAX_LIGHT_MATRICES);
  descriptor_builder.Reset();
  descriptor_builder.BindImages(0, shadow_images);
  descriptor_builder.BindStorageBuffer(1, matrix_buffer.buffer);
  descriptor_builder.BindSampler(2, shadow_sampler);
  descriptor_builder.Build(context, VK_SHADER_STAGE_ALL, shadow_descriptor_set,
                           shadow_descriptor_layout);
}

uint32_t LightManager::AddDirectionalLight(VulkanContext &context,
                                           glm::vec3 color, glm::vec3 direction,
                                           float intensity,
                                           ImmediateSubmit &immediate_submit) {
  DirectionalLight dir_light{};
  dir_light.color = color;
  dir_light.direction = glm::normalize(direction);
  dir_light.intensity = intensity;
  dir_light.cascade_index = matrix_index;

  directional_lights.push_back(glm::normalize(direction));

  UpdateBuffer(context, immediate_submit, &dir_light, sizeof(DirectionalLight),
               sizeof(DirectionalLight) * directional_light_index,
               directional_light_buffer);

  glm::vec3 light_dir = glm::normalize(direction);
  glm::vec3 light_pos =
      glm::zero<glm::vec3>() - light_dir * DIRECTIONAL_LIGHT_DISTANCE;
  glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);

  glm::mat4 light_view = glm::lookAt(light_pos, glm::zero<glm::vec3>(), up);

  std::array<glm::mat4, CASCADES.size()> light_matrices;

  for (uint32_t i = 0; i < CASCADES.size(); i++) {
    float cascade_size = CASCADES[i];
    glm::mat4 light_projection =
        glm::ortho(-cascade_size, cascade_size, -cascade_size, cascade_size,
                   DIRECTIONAL_LIGHT_NEAR_PLANE, DIRECTIONAL_LIGHT_FAR_PLANE);

    light_matrices[i] = light_projection * light_view;

    CreateAllocatedImage(context, SHADOW_IMAGE_EXTENT, VK_FORMAT_D32_SFLOAT,
                         VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
                             VK_IMAGE_USAGE_SAMPLED_BIT,
                         shadow_images[matrix_index + i]);

    VkDescriptorImageInfo image_info{};
    image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    image_info.imageView = shadow_images[matrix_index + i].image_view;

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    write.pImageInfo = &image_info;
    write.dstBinding = 0;
    write.dstArrayElement = matrix_index + i;
    write.dstSet = shadow_descriptor_set;

    vkUpdateDescriptorSets(context.device, 1, &write, 0, nullptr);
  }

  UpdateBuffer(context, immediate_submit, light_matrices.data(),
               sizeof(glm::mat4) * light_matrices.size(),
               sizeof(glm::mat4) * matrix_index, matrix_buffer);

  matrix_index += CASCADES.size();

  return directional_light_index++;
}

void LightManager::UpdateDirectionalLight(VulkanContext &context,
                                          glm::vec3 direction, uint32_t index,
                                          ImmediateSubmit &immediate_submit) {
  direction = glm::normalize(direction);
  assert(index <= directional_lights.size() &&
         "attempt to update out of bounds directional light");
  directional_lights[index] = direction;
  uint32_t direction_offset = offsetof(DirectionalLight, direction);
  UpdateBuffer(context, immediate_submit, &direction,
               sizeof(direction),
               (index * sizeof(DirectionalLight)) + direction_offset,
               directional_light_buffer);
}

void LightManager::UpdateMatrices(VulkanContext &context,
                                  ImmediateSubmit &immediate_submit,
                                  glm::vec3 camera_position) {

  std::vector<glm::mat4> matrices;
  matrices.reserve(directional_lights.size() * CASCADES.size());

  std::array<glm::mat4, CASCADES.size()> cascade_projection_matrices;

  for (uint32_t i = 0; i < CASCADES.size(); i++) {
    float cascade_size = CASCADES[i];
    cascade_projection_matrices[i] =
        glm::ortho(-cascade_size, cascade_size, -cascade_size, cascade_size,
                   DIRECTIONAL_LIGHT_NEAR_PLANE, DIRECTIONAL_LIGHT_FAR_PLANE);
  }

  const glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);

  for (auto &direction : directional_lights) {
    glm::vec3 light_pos =
      glm::vec3(camera_position.x, 0, camera_position.z) - direction * DIRECTIONAL_LIGHT_DISTANCE;

    glm::mat4 light_view = glm::lookAt(light_pos, camera_position, up);

    for (uint32_t i = 0; i < CASCADES.size(); i++) {
      matrices.push_back(cascade_projection_matrices[i] * light_view);
    }
  }

  UpdateBuffer(context, immediate_submit, matrices.data(),
               sizeof(glm::mat4) * matrices.size(), 0, matrix_buffer);
}

uint32_t LightManager::AddPointLight(VulkanContext &context, glm::vec3 color,
                                     glm::vec3 position, float intensity,
                                     ImmediateSubmit &immediate_submit) {
  PointLight point_light{};
  point_light.position = position;
  point_light.color = color;
  point_light.intensity = intensity;

  UpdateBuffer(context, immediate_submit, &point_light, sizeof(PointLight),
               sizeof(PointLight) * point_light_index, point_light_buffer);

  return point_light_index++;
}

void LightManager::Destroy(VulkanContext &context) {
  for (auto &image : shadow_images) {
    DestroyAllocatedImage(context, image);
  }
  DestroyImageSampler(context, shadow_sampler);

  vkDestroyDescriptorSetLayout(context.device, light_descriptor_layout,
                               nullptr);
  vkDestroyDescriptorSetLayout(context.device, shadow_descriptor_layout,
                               nullptr);

  DestroyBuffer(context, matrix_buffer);
  DestroyBuffer(context, point_light_buffer);
  DestroyBuffer(context, directional_light_buffer);
}
