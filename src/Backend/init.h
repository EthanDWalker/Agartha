#pragma once
#include <vulkan/vulkan.h>

namespace vkinit {
VkCommandBufferBeginInfo
CommandBufferBeginInfo(VkCommandBufferUsageFlags flags);
VkImageSubresourceRange ImageSubresourceRange(VkImageAspectFlags aspect_mask);
VkSemaphoreSubmitInfo SemaphoreSubmitInfo(VkPipelineStageFlags2 stage_mask,
                                          VkSemaphore semaphore);
VkCommandBufferSubmitInfo CommandBufferSubmitInfo(VkCommandBuffer cmd);
VkSubmitInfo2 SubmitInfo(VkCommandBufferSubmitInfo *cmd,
                         VkSemaphoreSubmitInfo *signal_semaphore_info,
                         VkSemaphoreSubmitInfo *wait_semaphore_info);
VkImageCreateInfo ImageCI(VkFormat format, VkImageUsageFlags usage_flags,
                          VkExtent3D extent);
VkImageViewCreateInfo
ImageViewCI(VkFormat format, VkImageAspectFlags aspect_flags, VkImage image);
} // namespace vkinit
