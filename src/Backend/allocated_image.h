#pragma once
#include "Backend/context.h"
#include <vma/vk_mem_alloc.h>
#include <vulkan/vulkan.h>

struct AllocatedImage {
  VkExtent3D extent;
  VkImage image;
  VkImageView image_view;
  VmaAllocation allocation;
  VkFormat format;
};

void GenerateMipmaps(VkCommandBuffer cmd, AllocatedImage &image);

void CreateImageSampler(VulkanContext &context, VkSampler &sampler);

void DestroyImageSampler(VulkanContext &context, VkSampler &sampler);

void CreateAllocatedImage(
    VulkanContext &context, VkExtent3D size, VkFormat format,
    VkImageUsageFlags usage_flags, AllocatedImage &image,
    bool mipmapped = false, bool cube_map = false,
    VkSampleCountFlagBits sample_count = VK_SAMPLE_COUNT_1_BIT);

void TransitionImage(VkCommandBuffer cmd, VkImageLayout old_layout,
                     VkImageLayout new_layout, VkImage image,
                     uint32_t mip_levels = 1, bool depth = false);

void CopyImageToImage(VkCommandBuffer cmd, VkImage source, VkImage destination,
                      VkExtent2D src_size, VkExtent2D dst_size);

void DestroyAllocatedImage(VulkanContext &context, AllocatedImage &image);
