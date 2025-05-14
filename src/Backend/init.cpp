#include "init.h"

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
} // namespace vkinit
