#pragma once

#include "context.h"
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

class DescriptorAllocatator {
public:
  struct PoolSizes {
    std::vector<std::pair<VkDescriptorType, float>> sizes = {
        {VK_DESCRIPTOR_TYPE_SAMPLER, 0.5f},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4.f},
        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 4.f},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1.f},
        {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1.f},
        {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1.f},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2.f},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2.f},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1.f},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1.f},
        {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 0.5f}};
  };

  void ResetPools();
  bool Allocate(VkDescriptorSet *set, VkDescriptorSetLayout layout);

  void Init(VulkanContext &context);

  void Cleanup();

  VkDevice device;

private:
  VkDescriptorPool GrabPool();

  VkDescriptorPool m_current_pool{VK_NULL_HANDLE};
  PoolSizes m_descriptor_sizes;
  std::vector<VkDescriptorPool> m_used_pools;
  std::vector<VkDescriptorPool> m_free_pools;
};

class DescriptorLayoutCache {
public:
  void Init(VulkanContext &context);
  void Cleanup();

  VkDescriptorSetLayout
  CreateDescriptorLayout(VkDescriptorSetLayoutCreateInfo *info);

  struct DescriptorLayoutInfo {
    std::vector<VkDescriptorSetLayoutBinding> bindings;

    bool operator==(const DescriptorLayoutInfo &other) const;

    size_t hash() const;
  };

private:
  struct DescriptorLayoutHash {
    size_t operator()(const DescriptorLayoutInfo &k) const { return k.hash(); }
  };

  std::unordered_map<DescriptorLayoutInfo, VkDescriptorSetLayout, DescriptorLayoutHash> layout_cache;
  VkDevice device;
};
