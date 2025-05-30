#pragma once

#include "Backend/allocated_image.h"
#include "Backend/context.h"
#include "Backend/descriptors.h"
#include "Loaders/model.h"
#include "types.h"
#include <filesystem>
#include <string>
#include <unordered_map>

static const std::filesystem::path texture_dir = "../assets/textures/";

struct TextureManager {
  const uint32_t MAX_TEXTURES = 1024;

  VkDescriptorSet descriptor_set;
  VkDescriptorSetLayout descriptor_set_layout;

  std::vector<AllocatedImage> texture_data;
  std::unordered_map<std::string, uint32_t> texture_indices;

  void Init(VulkanContext &context, DescriptorBuilder &descriptor_builder);
  Material GetMaterial(MaterialData data);
  void Destroy(VulkanContext &context);
};
