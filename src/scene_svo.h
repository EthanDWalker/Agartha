#pragma once

#include "Backend/buffer.h"
#include "Backend/context.h"
#include "Backend/descriptors.h"
#include "Backend/pipeline.h"
#include "Managers/light_manager.h"
#include "Managers/scene_manager.h"
#include "Managers/texture_manager.h"
#include "camera.h"

const uint32_t SVO_DEPTH = 5;
const VkExtent3D SVO_EXTENT = {100, 100, 100};

struct SvoNode {
  glm::vec3 color;
  uint32_t visible;
};

struct SceneSvo {
  AllocatedBuffer levels[SVO_DEPTH];

  AllocatedBuffer draw_buffer;
  AllocatedBuffer draw_count_buffer;

  AllocatedBuffer data_buffer;

  Pipeline build_pipeline;
  Pipeline build_draw_buffer_pipeline;
  Pipeline debug_pipeline;

  VkDescriptorSet draw_buffer_descriptor_set;
  VkDescriptorSetLayout draw_buffer_descriptor_layout;

  VkDescriptorSet svo_descriptor_set;
  VkDescriptorSetLayout svo_descriptor_layout;

  void Create(VulkanContext &context, SceneManager &scene_manager,
              LightManager &light_manager, TextureManager &texture_manager,
              Camera &camera, DescriptorBuilder &descriptor_builder);

  void BuildDrawCommands(VkCommandBuffer cmd, SceneManager &scene_manager);

  void Build(VkCommandBuffer cmd, SceneManager &scene_manager,
             TextureManager &texture_manager, LightManager &light_manager);

  void DrawDebugView(VkCommandBuffer cmd, SceneManager &scene_manager,
                     Camera &camera, AllocatedImage &draw_image,
                     AllocatedImage &depth_image, VkImageLayout new_layout);

  void Destroy(VulkanContext &context);
};
