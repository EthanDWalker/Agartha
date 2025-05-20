#pragma once
#include "Backend/context.h"
#include "Backend/image.h"
#include <array>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>
#include <vulkan/vulkan.h>

struct DescriptorPool {
  static constexpr std::array<std::pair<VkDescriptorType, float>, 5>
      pool_ratios{{
          {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1.0f},
          {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3.0f},
          {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 3.0f},
          {VK_DESCRIPTOR_TYPE_SAMPLER, 0.1f},
          {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1.0f},
      }};

  uint32_t alloc_scaler{100};

  VkDescriptorPool current_pool;
  std::vector<VkDescriptorPool> used_pools;

  void Init(VulkanContext &context);

  void Allocate(VulkanContext &context, VkDescriptorSetLayout &layout,
                VkDescriptorSet &set, void *pNext = nullptr);

  void NewPool(VulkanContext &context);

  void Destroy(VulkanContext &context);
};

struct DesciptorBuilder {
  DescriptorPool pool;

  std::vector<VkDescriptorSetLayoutBinding> bindings;
  std::vector<void *> writes;

  void Init(VulkanContext &context);

  void BindBuffer(uint32_t binding, VkBuffer buffer);
  void BindCombinedImage(uint32_t binding, VkImageView image_view,
                         VkSampler sampler);
  void BindSampler(uint32_t binding, VkSampler sampler);
  void BindImages(uint32_t binding, std::span<AllocatedImage> image_views);

  void Build(VulkanContext &context, VkShaderStageFlags stage_flags,
             VkDescriptorSet &set, VkDescriptorSetLayout &layout);

  void Reset();

  void Destroy(VulkanContext &context);
};
