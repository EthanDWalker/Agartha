#pragma once
#include "Backend/context.h"
#include <vma/vk_mem_alloc.h>
#include <vulkan/vulkan.h>

struct AllocatedImage {
  VkImage image;
  VkImageView imageView;
  VmaAllocation allocation;
  VkExtent3D imageExtent;
  VkFormat imageFormat;
};

void CreateAllocatedImage(VulkanContext &context, AllocatedImage &image);

void TransitionImage(VkCommandBuffer cmd, VkImageLayout old_format,
                     VkImageLayout new_format, VkImage image);

void DestroyAllocatedImage(VulkanContext &context, AllocatedImage &image);
