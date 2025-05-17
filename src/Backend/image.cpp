#include "image.h"
#include "Backend/buffer.h"
#include "Backend/context.h"
#include "Backend/immediate_submit.h"
#include "Backend/init.h"
#include "Backend/util.h"
#include <cstdint>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

void CreateImageSampler(VulkanContext &context, VkSampler &sampler) {
  VkSamplerCreateInfo sampler_ci{};
  sampler_ci.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  sampler_ci.magFilter = VK_FILTER_LINEAR;
  sampler_ci.minFilter = VK_FILTER_LINEAR;

  VK_CHECK(vkCreateSampler(context.device, &sampler_ci, nullptr, &sampler));
}

void DestroyImageSampler(VulkanContext &context, VkSampler &sampler) {
  vkDestroySampler(context.device, sampler, nullptr);
}

void CreateAllocatedImage(VulkanContext &context, VkExtent3D size,
                          VkFormat format, VkImageUsageFlags usage_flags,
                          AllocatedImage &image) {
  image.format = format;
  image.extent = size;

  VkImageCreateInfo image_ci = vkinit::ImageCI(format, usage_flags, size);

  VmaAllocationCreateInfo alloc_info{};
  alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;
  alloc_info.requiredFlags =
      VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  VK_CHECK(vmaCreateImage(context.allocator, &image_ci, &alloc_info,
                          &image.image, &image.allocation, nullptr));

  VkImageAspectFlags aspect_flags = VK_IMAGE_ASPECT_COLOR_BIT;
  if (format == VK_FORMAT_D32_SFLOAT) {
    aspect_flags = VK_IMAGE_ASPECT_DEPTH_BIT;
  }

  VkImageViewCreateInfo image_view_ci =
      vkinit::ImageViewCI(format, aspect_flags, image.image);
  VK_CHECK(vkCreateImageView(context.device, &image_view_ci, nullptr,
                             &image.image_view));
}

void CreateAllocatedImageData(VulkanContext &context,
                              ImmediateSubmit immediate_submit, void *data,
                              VkExtent3D size, VkFormat format,
                              VkImageUsageFlags usage_flags,
                              AllocatedImage &image) {
  const uint8_t channel_count = 4; // HARDCODE
  size_t data_size = size.depth * size.width * size.height * channel_count;

  AllocatedBuffer upload_buffer;
  CreateBuffer(context, data_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
               VMA_MEMORY_USAGE_CPU_TO_GPU, upload_buffer);

  memcpy(upload_buffer.info.pMappedData, data, data_size);

  CreateAllocatedImage(context, size, format,
                       usage_flags | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                           VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                       image);

  immediate_submit.Submit(context, [&](VkCommandBuffer cmd) {
    TransitionImage(cmd, VK_IMAGE_LAYOUT_UNDEFINED,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, image.image);
    VkBufferImageCopy copy_region{};
    copy_region.bufferOffset = 0;
    copy_region.bufferRowLength = 0;
    copy_region.bufferImageHeight = 0;
    copy_region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copy_region.imageSubresource.mipLevel = 0;
    copy_region.imageSubresource.baseArrayLayer = 0;
    copy_region.imageSubresource.layerCount = 1;
    copy_region.imageExtent = size;

    vkCmdCopyBufferToImage(cmd, upload_buffer.buffer, image.image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                           &copy_region);

    TransitionImage(cmd, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, image.image);
  });

  DestroyBuffer(context, upload_buffer);
}

void TransitionImage(VkCommandBuffer cmd, VkImageLayout old_layout,
                     VkImageLayout new_layout, VkImage image) {
  VkImageMemoryBarrier2 image_barrier{};
  image_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
  image_barrier.image = image;
  image_barrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
  image_barrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
  image_barrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
  image_barrier.dstAccessMask =
      VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT;

  image_barrier.oldLayout = old_layout;
  image_barrier.newLayout = new_layout;

  VkImageAspectFlags aspect_mask =
      (new_layout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL)
          ? VK_IMAGE_ASPECT_DEPTH_BIT
          : VK_IMAGE_ASPECT_COLOR_BIT;

  image_barrier.subresourceRange = vkinit::ImageSubresourceRange(aspect_mask);
  image_barrier.image = image;

  VkDependencyInfo dep_info{};
  dep_info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
  dep_info.imageMemoryBarrierCount = 1;
  dep_info.pImageMemoryBarriers = &image_barrier;

  vkCmdPipelineBarrier2(cmd, &dep_info);
}

void CopyImageToImage(VkCommandBuffer cmd, VkImage source, VkImage destination,
                      VkExtent2D src_size, VkExtent2D dst_size) {
  VkImageBlit2 blit_region{};
  blit_region.sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2;

  blit_region.srcOffsets[1].x = src_size.width;
  blit_region.srcOffsets[1].y = src_size.height;
  blit_region.srcOffsets[1].z = 1;

  blit_region.dstOffsets[1].x = dst_size.width;
  blit_region.dstOffsets[1].y = dst_size.height;
  blit_region.dstOffsets[1].z = 1;

  blit_region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  blit_region.srcSubresource.baseArrayLayer = 0;
  blit_region.srcSubresource.layerCount = 1;
  blit_region.srcSubresource.mipLevel = 0;

  blit_region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  blit_region.dstSubresource.baseArrayLayer = 0;
  blit_region.dstSubresource.layerCount = 1;
  blit_region.dstSubresource.mipLevel = 0;

  VkBlitImageInfo2 blit_info{.sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2,
                             .pNext = nullptr};
  blit_info.dstImage = destination;
  blit_info.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  blit_info.srcImage = source;
  blit_info.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
  blit_info.filter = VK_FILTER_LINEAR;
  blit_info.regionCount = 1;
  blit_info.pRegions = &blit_region;

  vkCmdBlitImage2(cmd, &blit_info);
}

void DestroyAllocatedImage(VulkanContext &context, AllocatedImage &image) {
  vmaDestroyImage(context.allocator, image.image, image.allocation);
  vkDestroyImageView(context.device, image.image_view, nullptr);
}
