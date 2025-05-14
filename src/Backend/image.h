#pragma once
#include "Backend/context.h"
#include <vma/vk_mem_alloc.h>
#include <vulkan/vulkan.h>

struct AllocatedImage {
  VkImage image;
  VkImageView image_view;
  VmaAllocation allocation;
  VkExtent3D extent;
  VkFormat format;
};

void CreateAllocatedImage(VulkanContext &context, VkExtent3D size,
                          VkFormat format, VkImageUsageFlags flags,
                          AllocatedImage &image);

void TransitionImage(VkCommandBuffer cmd, VkImageLayout old_format,
                     VkImageLayout new_format, VkImage image);

void CopyImageToImage(VkCommandBuffer cmd, VkImage source, VkImage destination,
                      VkExtent2D src_size, VkExtent2D dst_size);

void DestroyAllocatedImage(VulkanContext &context, AllocatedImage &image);
