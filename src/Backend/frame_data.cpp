#include "frame_data.h"
#include "Backend/context.h"
#include "Backend/util.h"

void CreateFrameData(FrameData &frame_data) {
  VkCommandPoolCreateInfo command_pool_ci{};
  command_pool_ci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  command_pool_ci.queueFamilyIndex = VulkanContext::graphics_queue_index;
  command_pool_ci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

  VK_CHECK(vkCreateCommandPool(VulkanContext::device, &command_pool_ci, nullptr,
                               &frame_data.command_pool));

  VkCommandBufferAllocateInfo command_buffer_ci{};
  command_buffer_ci.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  command_buffer_ci.commandBufferCount = 1;
  command_buffer_ci.commandPool = frame_data.command_pool;
  command_buffer_ci.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

  VK_CHECK(vkAllocateCommandBuffers(VulkanContext::device, &command_buffer_ci,
                                    &frame_data.command_buffer));

  VkFenceCreateInfo fence_ci{};
  fence_ci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fence_ci.flags = VK_FENCE_CREATE_SIGNALED_BIT;

  VK_CHECK(vkCreateFence(VulkanContext::device, &fence_ci, nullptr, &frame_data.render_fence));

  VkSemaphoreCreateInfo semaphore_ci{};
  semaphore_ci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

  VK_CHECK(vkCreateSemaphore(VulkanContext::device, &semaphore_ci, nullptr,
                             &frame_data.render_semaphore));

  VK_CHECK(vkCreateSemaphore(VulkanContext::device, &semaphore_ci, nullptr,
                             &frame_data.swapchain_semaphore));
}

void DestroyFrameData(FrameData &frame_data) {
  vkDestroyCommandPool(VulkanContext::device, frame_data.command_pool, nullptr);

  vkDestroySemaphore(VulkanContext::device, frame_data.swapchain_semaphore, nullptr);
  vkDestroySemaphore(VulkanContext::device, frame_data.render_semaphore, nullptr);

  vkDestroyFence(VulkanContext::device, frame_data.render_fence, nullptr);
}
