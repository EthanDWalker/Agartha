#pragma once
#include "Backend/context.h"
#include "fmt/base.h"
#include <array>
#include <cstdint>
#include <unordered_map>
#include <utility>
#include <vector>
#include <vulkan/vulkan.h>

struct DescriptorPool {
  static constexpr std::array<std::pair<VkDescriptorType, float>, 3>
      pool_ratios{{
          {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1.0f},
          {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3.0f},
          {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1.0f},
      }};

  uint32_t alloc_scaler{100};

  VkDescriptorPool current_pool;
  std::vector<VkDescriptorPool> used_pools;

  void Init(VulkanContext &context);

  void Allocate(VulkanContext &context, VkDescriptorSetLayout &layout,
                VkDescriptorSet &set);

  void NewPool(VulkanContext &context);

  void Destroy(VulkanContext &context);
};

struct DesciptorBuilder {
  DescriptorPool pool;

  std::vector<VkDescriptorSetLayoutBinding> bindings;
  std::unordered_map<uint32_t, VkDescriptorBufferInfo> buffer_writes;
  std::unordered_map<uint32_t, VkDescriptorImageInfo> image_writes;

  void Init(VulkanContext &context);

  void BindBuffer(uint32_t binding, VkBuffer buffer);
  void BindImage(uint32_t binding, VkImageView image_view, VkSampler sampler);

  void Build(VulkanContext &context, VkShaderStageFlags stage_flags,
             VkDescriptorSet &set, VkDescriptorSetLayout &layout);

  void Reset();

  void Destroy(VulkanContext &context);
};
