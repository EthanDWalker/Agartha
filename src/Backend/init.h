#pragma once
#include <Volk/volk.h>
#include <span>

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
                          VkExtent3D extent, uint32_t mip_levels = 1);
VkImageViewCreateInfo ImageViewCI(VkFormat format,
                                  VkImageAspectFlags aspect_flags,
                                  VkImage image, uint32_t mip_levels);
VkRenderingAttachmentInfo AttachmentInfo(VkImageView view,
                                         VkImageView resolve_view,
                                         VkClearValue *clear,
                                         VkImageLayout layout);
VkRenderingAttachmentInfo DepthAttachmentInfo(VkImageView image_view,
                                              VkImageLayout layout);
VkRenderingInfo
RenderingInfo(VkExtent3D render_extent,
              std::span<VkRenderingAttachmentInfo> color_attachments,
              VkRenderingAttachmentInfo *depth_attachment);
VkViewport Viewport(VkExtent3D extent);
VkRect2D Scissor(VkExtent3D extent);
} // namespace vkinit
