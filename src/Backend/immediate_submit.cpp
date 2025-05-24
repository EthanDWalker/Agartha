#include "immediate_submit.h"
#include "Backend/init.h"
#include "Backend/util.h"
#include <limits>

void ImmediateSubmit::SubmitAsync(
    VulkanContext &context,
    std::function<void(VkCommandBuffer cmd)> &&function) {
  VkCommandPool command_pool;
  VkCommandBuffer command_buffer;
  VkFence fence;

  VkCommandPoolCreateInfo command_pool_ci{};
  command_pool_ci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  command_pool_ci.queueFamilyIndex = context.graphics_queue_index;
  command_pool_ci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

  VK_CHECK(vkCreateCommandPool(context.device, &command_pool_ci, nullptr,
                               &command_pool));

  VkCommandBufferAllocateInfo command_buffer_ci{};
  command_buffer_ci.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  command_buffer_ci.commandBufferCount = 1;
  command_buffer_ci.commandPool = command_pool;
  command_buffer_ci.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

  VK_CHECK(vkAllocateCommandBuffers(context.device, &command_buffer_ci,
                                    &command_buffer));

  VkFenceCreateInfo fence_ci{};
  fence_ci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fence_ci.flags = VK_FENCE_CREATE_SIGNALED_BIT;

  VK_CHECK(vkCreateFence(context.device, &fence_ci, nullptr, &fence));

  VK_CHECK(vkResetFences(context.device, 1, &fence));
  VK_CHECK(vkResetCommandBuffer(command_buffer, 0));

  VkCommandBufferBeginInfo cmd_begin_info = vkinit::CommandBufferBeginInfo(
      VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

  VK_CHECK(vkBeginCommandBuffer(command_buffer, &cmd_begin_info));

  function(command_buffer);

  VK_CHECK(vkEndCommandBuffer(command_buffer));

  VkCommandBufferSubmitInfo cmd_submit_info =
      vkinit::CommandBufferSubmitInfo(command_buffer);
  VkSubmitInfo2 submit_info =
      vkinit::SubmitInfo(&cmd_submit_info, nullptr, nullptr);

  VK_CHECK(vkQueueSubmit2(context.graphics_queue, 1, &submit_info, fence));

  VK_CHECK(vkWaitForFences(context.device, 1, &fence, VK_TRUE,
                           std::numeric_limits<uint32_t>::max()));

  vkDestroyCommandPool(context.device, command_pool, nullptr);
  vkDestroyFence(context.device, fence, nullptr);
}

void ImmediateSubmit::Create(VulkanContext &context) {
  VkCommandPoolCreateInfo command_pool_ci{};
  command_pool_ci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  command_pool_ci.queueFamilyIndex = context.graphics_queue_index;
  command_pool_ci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

  VK_CHECK(vkCreateCommandPool(context.device, &command_pool_ci, nullptr,
                               &command_pool));

  VkCommandBufferAllocateInfo command_buffer_ci{};
  command_buffer_ci.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  command_buffer_ci.commandBufferCount = 1;
  command_buffer_ci.commandPool = command_pool;
  command_buffer_ci.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

  VK_CHECK(vkAllocateCommandBuffers(context.device, &command_buffer_ci,
                                    &command_buffer));

  VkFenceCreateInfo fence_ci{};
  fence_ci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fence_ci.flags = VK_FENCE_CREATE_SIGNALED_BIT;

  VK_CHECK(vkCreateFence(context.device, &fence_ci, nullptr, &fence));
}

void ImmediateSubmit::Submit(
    VulkanContext &context,
    std::function<void(VkCommandBuffer cmd)> &&function) {
  VK_CHECK(vkResetFences(context.device, 1, &fence));
  VK_CHECK(vkResetCommandBuffer(command_buffer, 0));

  VkCommandBufferBeginInfo cmd_begin_info = vkinit::CommandBufferBeginInfo(
      VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

  VK_CHECK(vkBeginCommandBuffer(command_buffer, &cmd_begin_info));

  function(command_buffer);

  VK_CHECK(vkEndCommandBuffer(command_buffer));

  VkCommandBufferSubmitInfo cmd_submit_info =
      vkinit::CommandBufferSubmitInfo(command_buffer);
  VkSubmitInfo2 submit_info =
      vkinit::SubmitInfo(&cmd_submit_info, nullptr, nullptr);

  VK_CHECK(vkQueueSubmit2(context.graphics_queue, 1, &submit_info, fence));

  VK_CHECK(vkWaitForFences(context.device, 1, &fence, VK_TRUE,
                           std::numeric_limits<uint32_t>::max()));
}

void ImmediateSubmit::Destroy(VulkanContext &context) {
  vkDestroyCommandPool(context.device, command_pool, nullptr);
  vkDestroyFence(context.device, fence, nullptr);
}
