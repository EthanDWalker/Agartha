#pragma once
#include "Backend/context.h"
#include "Backend/immediate_submit.h"
#include <vma/vk_mem_alloc.h>
#include <vulkan/vulkan.h>

struct AllocatedImage {
  VkImage image;
  VkImageView image_view;
  VmaAllocation allocation;
  VkExtent3D extent;
  VkFormat format;
};

void GenerateMipmaps(VulkanContext &context, ImmediateSubmit &immediate_submit,
                     uint32_t mipLevels, AllocatedImage &image);

void CreateImageSampler(VulkanContext &context, VkSampler &sampler);

void DestroyImageSampler(VulkanContext &context, VkSampler &sampler);

void CreateAllocatedImage(VulkanContext &context, VkExtent3D size,
                          VkFormat format, VkImageUsageFlags usage_flags,
                          AllocatedImage &image, uint32_t mip_levels = 1,
                          bool cube_map = false);

void CreateAllocatedImageData(VulkanContext &context,
                              ImmediateSubmit immediate_submit, void *data,
                              VkExtent3D size, VkFormat format,
                              VkImageUsageFlags usage_flags,
                              AllocatedImage &image);

void TransitionImage(VkCommandBuffer cmd, VkImageLayout old_format,
                     VkImageLayout new_format, VkImage image);

void CopyImageToImage(VkCommandBuffer cmd, VkImage source, VkImage destination,
                      VkExtent2D src_size, VkExtent2D dst_size);

void DestroyAllocatedImage(VulkanContext &context, AllocatedImage &image);
