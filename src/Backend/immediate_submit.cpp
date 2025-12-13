#include "immediate_submit.h"
#include "Backend/context.h"
#include "Backend/init.h"
#include "Backend/util.h"
#include <cassert>
#include <cstdint>
#include <limits>
#include <mutex>

thread_local ImmediateSubmit::ThreadData ImmediateSubmit::thread_data = {
    .command_pool = VK_NULL_HANDLE,
};

void ImmediateSubmit::Submit(std::function<void(VkCommandBuffer cmd)> &&function) {
  if (thread_data.command_pool == VK_NULL_HANDLE) {
    VkCommandPoolCreateInfo command_pool_ci{};
    command_pool_ci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    command_pool_ci.queueFamilyIndex = VulkanContext::graphics_queue_index;
    command_pool_ci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    VK_CHECK(vkCreateCommandPool(VulkanContext::device, &command_pool_ci, nullptr,
                                 &thread_data.command_pool));

    VkCommandBufferAllocateInfo command_buffer_ci{};
    command_buffer_ci.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    command_buffer_ci.commandBufferCount = 1;
    command_buffer_ci.commandPool = thread_data.command_pool;
    command_buffer_ci.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

    VK_CHECK(vkAllocateCommandBuffers(VulkanContext::device, &command_buffer_ci,
                                      &thread_data.command_buffer));

    VkFenceCreateInfo fence_ci{};
    fence_ci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_ci.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    VK_CHECK(vkCreateFence(VulkanContext::device, &fence_ci, nullptr, &thread_data.fence));
  }

  VK_CHECK(vkResetFences(VulkanContext::device, 1, &thread_data.fence));
  VK_CHECK(vkResetCommandBuffer(thread_data.command_buffer, 0));

  VkCommandBufferBeginInfo cmd_begin_info =
      vkinit::CommandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

  VK_CHECK(vkBeginCommandBuffer(thread_data.command_buffer, &cmd_begin_info));

  function(thread_data.command_buffer);

  VK_CHECK(vkEndCommandBuffer(thread_data.command_buffer));

  VkCommandBufferSubmitInfo cmd_submit_info =
      vkinit::CommandBufferSubmitInfo(thread_data.command_buffer);
  VkSubmitInfo2 submit_info = vkinit::SubmitInfo(&cmd_submit_info, nullptr, nullptr);

  {
    std::lock_guard<std::mutex> lock(VulkanContext::graphics_queue_mutex);

    VK_CHECK(vkQueueSubmit2(VulkanContext::graphics_queue, 1, &submit_info, thread_data.fence));
  }

  VK_CHECK(vkWaitForFences(VulkanContext::device, 1, &thread_data.fence, VK_TRUE,
                           std::numeric_limits<uint64_t>::max()));
}
