#pragma once
#include "Backend/context.h"
#include <mutex>
#include <vma/vk_mem_alloc.h>
#include <vulkan/vulkan.h>

struct AllocatedImage {
  VkImage image;
  VkImageView image_view;
  VmaAllocation allocation;
  VkExtent3D extent;
  VkFormat format;
};

void GenerateMipmaps(VulkanContext &context, uint32_t mipLevels,
                     AllocatedImage &image);

void CreateImageSampler(VulkanContext &context, VkSampler &sampler);

void DestroyImageSampler(VulkanContext &context, VkSampler &sampler);

void CreateAllocatedImage(
    VulkanContext &context, VkExtent3D size, VkFormat format,
    VkImageUsageFlags usage_flags, AllocatedImage &image,
    uint32_t mip_levels = 1, bool cube_map = false,
    VkSampleCountFlagBits sample_count = VK_SAMPLE_COUNT_1_BIT);

void TransitionImage(VkCommandBuffer cmd, VkImageLayout old_layout,
                     VkImageLayout new_layout, VkImage image,
                     uint32_t mip_levels = 1);

void CopyImageToImage(VkCommandBuffer cmd, VkImage source, VkImage destination,
                      VkExtent2D src_size, VkExtent2D dst_size);

void DestroyAllocatedImage(VulkanContext &context, AllocatedImage &image);
