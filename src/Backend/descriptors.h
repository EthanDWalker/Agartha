#pragma once
#include "Backend/allocated_image.h"
#include "Backend/buffer.h"
#include "Backend/context.h"
#include <array>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

void UpdateDescriptorSetStorageImage(VulkanContext &vulkan_context,
                                     AllocatedImage &image,
                                     VkDescriptorSet descriptor_set,
                                     uint32_t binding, uint32_t index = 0);

struct DescriptorPool {
  static constexpr std::array<std::pair<VkDescriptorType, float>, 7>
      pool_ratios{{
          {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3.0f},
          {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 3.0f},
          {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1.0f},
          {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1.0f},
          {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1.0f},
          {VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1.0f},
          {VK_DESCRIPTOR_TYPE_SAMPLER, 0.1f},
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

struct DescriptorBuilder {
  DescriptorPool pool;

  std::vector<VkDescriptorSetLayoutBinding> bindings;
  std::vector<void *> writes;

  void Init(VulkanContext &context);

  void BindUniformBuffer(uint32_t binding, VkBuffer buffer);
  void BindStorageBuffer(uint32_t binding, VkBuffer buffer);
  void BindStorageBuffers(uint32_t binding, std::span<AllocatedBuffer> buffers);

  void BindCombinedImage(uint32_t binding, VkImageView image_view,
                         VkSampler sampler);
  void BindStorageImage(uint32_t binding, VkImageView image_view);
  void BindStorageImages(uint32_t binding,
                         std::vector<VkImageView> image_views);

  void BindSampler(uint32_t binding, VkSampler sampler);
  void BindImage(uint32_t binding, VkImageView image_view);
  void BindImages(uint32_t binding, std::span<AllocatedImage> image_views);

  void BindAccelerationStructure(uint32_t binding,
                                 VkAccelerationStructureKHR &as);

  void Build(VulkanContext &context, VkShaderStageFlags stage_flags,
             VkDescriptorSet &set, VkDescriptorSetLayout &layout);

  void Reset();

  void Destroy(VulkanContext &context);
};
