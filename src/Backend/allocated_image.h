#pragma once
#include "Backend/context.h"
#include <vma/vk_mem_alloc.h>
#include <volk.h>

struct AllocatedImage {
  VkExtent3D extent;
  VkImage image;
  VkImageView image_view;
  VmaAllocation allocation;
  VkFormat format;
};

uint32_t CalculateMipLevels(VkExtent3D image_extent);

void GenerateMipmaps(VkCommandBuffer cmd, AllocatedImage &image);

void CreateImageSampler(VulkanContext &context, VkSampler &sampler);

void DestroyImageSampler(VulkanContext &context, VkSampler &sampler);

void CreateAllocatedImage(
    VulkanContext &context, VkExtent3D size, VkFormat format,
    VkImageUsageFlags usage_flags, AllocatedImage &image,
    bool mipmapped = false, bool cube_map = false,
    VkSampleCountFlagBits sample_count = VK_SAMPLE_COUNT_1_BIT);

void CreateImageDataAsync(
    VulkanContext &context, void *data, uint8_t channel_count, VkExtent3D size,
    VkFormat format, VkImageUsageFlags usage_flags, AllocatedImage &image,
    bool mipmapped = false,
    VkSampleCountFlagBits sample_count = VK_SAMPLE_COUNT_1_BIT);

void TransitionImage(VkCommandBuffer cmd, VkAccessFlags2 src_access,
                     VkAccessFlags2 dst_access, VkPipelineStageFlags2 src_stage,
                     VkPipelineStageFlags2 dst_stage, VkImageLayout old_layout,
                     VkImageLayout new_layout, VkImage image, bool depth = false);

void CopyImageToImage(VkCommandBuffer cmd, VkImage source, VkImage destination,
                      VkExtent2D src_size, VkExtent2D dst_size);

void DestroyAllocatedImage(VulkanContext &context, AllocatedImage &image);
