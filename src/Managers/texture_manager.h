#pragma once

#include "Backend/allocated_image.h"
#include "Backend/context.h"
#include "Backend/descriptors.h"
#include "Backend/pipeline.h"
#include "Parsers/model.h"
#include "types.h"
#include <mutex>
#include <string>
#include <unordered_map>

const uint32_t MAX_TEXTURES = 1024;

struct TextureManager {
  VkDescriptorSet descriptor_set;
  VkDescriptorSetLayout descriptor_set_layout;

  VkDescriptorSet pack_input_descriptor_set;
  VkDescriptorSetLayout pack_input_descriptor_layout;

  VkDescriptorSet pack_output_descriptor_set;
  VkDescriptorSetLayout pack_output_descriptor_layout;

  Pipeline pack_pipeline;

  std::vector<AllocatedImage> texture_data;
  std::unordered_map<std::string, uint32_t> texture_indices;
  std::mutex texture_mutex;

  VkSampler sampler;

  uint32_t texture_index;

  void Init(VulkanContext &context, DescriptorBuilder &descriptor_builder);
  uint32_t UploadTexture(VulkanContext &context, std::string filename);
  uint32_t AddAllocatedImage(VulkanContext &context, AllocatedImage &image);
  Material UploadMaterial(VulkanContext &context,
                          MaterialData &data);
  void LoadTexture(VulkanContext &context, std::string filename,
                   AllocatedImage &image);
  void Destroy(VulkanContext &context);
};
