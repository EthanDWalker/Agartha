#pragma once

#include "Backend/allocated_image.h"
#include "Backend/context.h"
#include "Backend/descriptors.h"
#include "Loaders/model.h"
#include "types.h"
#include <filesystem>
#include <mutex>
#include <string>
#include <unordered_map>

static const std::filesystem::path texture_dir = "../assets/textures/";

const uint32_t MAX_TEXTURES = 1024;

struct TextureManager {
  VkDescriptorSet descriptor_set;
  VkDescriptorSetLayout descriptor_set_layout;

  std::vector<AllocatedImage> texture_data;
  std::unordered_map<std::string, uint32_t> texture_indices;
  std::mutex texture_mutex;

  VkSampler sampler;

  uint32_t texture_index;

  void Init(VulkanContext &context, DescriptorBuilder &descriptor_builder);
  uint32_t UploadTexture(VulkanContext &context, std::string filename);
  uint32_t AddAllocatedImage(VulkanContext &context, AllocatedImage &image);
  Material UploadMaterial(VulkanContext &context,
                          DescriptorBuilder &descriptor_builder,
                          MaterialData data);
  void LoadTexture(VulkanContext &context, std::string filename,
                   AllocatedImage &image);
  void Destroy(VulkanContext &context);
};
