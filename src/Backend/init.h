#pragma once
#include <vulkan/vulkan.h>

namespace vkinit {
VkCommandBufferBeginInfo
CommandBufferBeginInfo(VkCommandBufferUsageFlags flags);
VkImageSubresourceRange ImageSubresourceRange(VkImageAspectFlags aspect_mask);
VkSemaphoreSubmitInfo SemaphoreSumbitInfo(VkPipelineStageFlags2 stage_mask,
                                          VkSemaphore semaphore);
VkCommandBufferSubmitInfo CommandBufferSubmitInfo(VkCommandBuffer cmd);
VkSubmitInfo2 SubmitInfo(VkCommandBufferSubmitInfo *cmd,
                         VkSemaphoreSubmitInfo *signal_semaphore_info,
                         VkSemaphoreSubmitInfo *wait_semaphore_info);
} // namespace vkinit
