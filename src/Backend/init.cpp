#include "init.h"
#include <numbers>
#include <vulkan/vulkan_core.h>

namespace vkinit {
VkCommandBufferBeginInfo
CommandBufferBeginInfo(VkCommandBufferUsageFlags flags) {
  VkCommandBufferBeginInfo info{};
  info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  info.flags = flags;
  return info;
}

VkImageSubresourceRange ImageSubresourceRange(VkImageAspectFlags aspect_mask) {
  VkImageSubresourceRange info{};
  info.aspectMask = aspect_mask;
  info.baseMipLevel = 0;
  info.layerCount = VK_REMAINING_MIP_LEVELS;
  info.baseArrayLayer = 0;
  info.levelCount = VK_REMAINING_ARRAY_LAYERS;
  return info;
}

VkSemaphoreSubmitInfo SemaphoreSubmitInfo(VkPipelineStageFlags2 stage_mask,
                                          VkSemaphore semaphore) {
  VkSemaphoreSubmitInfo info{};
  info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
  info.semaphore = semaphore;
  info.stageMask = stage_mask;
  info.value = 1;
  return info;
}

VkCommandBufferSubmitInfo CommandBufferSubmitInfo(VkCommandBuffer cmd) {
  VkCommandBufferSubmitInfo info{};
  info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
  info.commandBuffer = cmd;
  return info;
}

VkSubmitInfo2 SubmitInfo(VkCommandBufferSubmitInfo *cmd,
                         VkSemaphoreSubmitInfo *signal_semaphore_info,
                         VkSemaphoreSubmitInfo *wait_semaphore_info) {
  VkSubmitInfo2 info{};
  info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
  info.pCommandBufferInfos = cmd;
  info.commandBufferInfoCount = 1;
  info.pSignalSemaphoreInfos = signal_semaphore_info;
  info.signalSemaphoreInfoCount = signal_semaphore_info ? 1 : 0;
  info.pWaitSemaphoreInfos = wait_semaphore_info;
  info.waitSemaphoreInfoCount = wait_semaphore_info ? 1 : 0;
  return info;
}

VkImageCreateInfo ImageCI(VkFormat format, VkImageUsageFlags usage_flags,
                          VkExtent3D extent) {
  VkImageCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  info.extent = extent;
  info.format = format;
  info.imageType = VK_IMAGE_TYPE_2D;
  info.mipLevels = 1;
  info.arrayLayers = 1;
  info.samples = VK_SAMPLE_COUNT_1_BIT;
  info.tiling = VK_IMAGE_TILING_OPTIMAL;
  info.usage = usage_flags;
  return info;
}

VkImageViewCreateInfo
ImageViewCI(VkFormat format, VkImageAspectFlags aspect_flags, VkImage image) {
  VkImageViewCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  info.viewType = VK_IMAGE_VIEW_TYPE_2D;
  info.image = image;
  info.format = format;
  info.subresourceRange.baseMipLevel = 0;
  info.subresourceRange.levelCount = 1;
  info.subresourceRange.baseArrayLayer = 0;
  info.subresourceRange.layerCount = 1;
  info.subresourceRange.aspectMask = aspect_flags;
  return info;
}
} // namespace vkinit
