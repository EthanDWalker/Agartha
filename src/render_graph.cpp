#include "render_graph.h"
#include "Backend/allocated_image.h"
#include "Backend/buffer.h"
#include "Backend/context.h"
#include "Backend/frame_data.h"
#include "Backend/init.h"
#include "Backend/swapchain.h"
#include "Backend/util.h"
#include "GLFW/glfw3.h"
#include <cstdint>
#include <limits>
#include <mutex>

void DependencyBuilder::AddBufferDependency(
    AllocatedBuffer buffer, VkAccessFlagBits2 src_access,
    VkAccessFlagBits2 dst_access, VkPipelineStageFlagBits2 src_stage,
    VkPipelineStageFlagBits2 dst_stage) {
  VkBufferMemoryBarrier2 barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
  barrier.buffer = buffer.buffer;
  barrier.size = VK_WHOLE_SIZE;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.srcAccessMask = src_access;
  barrier.dstAccessMask = dst_access;
  barrier.srcStageMask = src_stage;
  barrier.dstStageMask = dst_stage;
  dependency.buffer_deps.push_back(barrier);
}

void DependencyBuilder::AddImageDependency(
    AllocatedImage image, VkAccessFlagBits2 src_access,
    VkAccessFlagBits2 dst_access, VkPipelineStageFlagBits2 src_stage,
    VkPipelineStageFlagBits2 dst_stage, VkImageLayout old_layout,
    VkImageLayout new_layout, bool depth) {
  VkImageMemoryBarrier2 barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
  barrier.image = image.image;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.srcAccessMask = src_access;
  barrier.dstAccessMask = dst_access;
  barrier.srcStageMask = src_stage;
  barrier.dstStageMask = dst_stage;
  barrier.oldLayout = old_layout;
  barrier.newLayout = new_layout;
  if (depth) {
    barrier.subresourceRange =
        vkinit::ImageSubresourceRange(VK_IMAGE_ASPECT_DEPTH_BIT);
  } else {
    barrier.subresourceRange =
        vkinit::ImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT);
  }
  dependency.image_deps.push_back(barrier);
}

void DependencyBuilder::AddDependency(VkAccessFlagBits2 src_access,
                                      VkAccessFlagBits2 dst_access,
                                      VkPipelineStageFlagBits2 src_stage,
                                      VkPipelineStageFlagBits2 dst_stage) {
  VkMemoryBarrier2 barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
  barrier.srcAccessMask = src_access;
  barrier.dstAccessMask = dst_access;
  barrier.srcStageMask = src_stage;
  barrier.dstStageMask = dst_stage;
  dependency.memory_deps.push_back(barrier);
}

void RenderGraphBuilder::AddPass(uint32_t level, Dependency dependency,
                                 std::function<void(VkCommandBuffer)> callback,
                                 bool *condition) {
  RenderPass render_pass{};
  render_pass.callback = callback;
  render_pass.dependency = dependency;
  render_pass.condition = condition;

  if (render_graph.size() <= level) {
    render_graph.resize(level + 1);
  }
  render_graph[level].push_back(render_pass);
}

void RenderGraph::Init(VulkanContext &context, GLFWwindow *window) {
  int32_t width, height;
  glfwGetWindowSize(window, &width, &height);
  CreateVulkanSwapchain(context, width, height, swapchain);
  VkExtent3D draw_image_extent = {
      swapchain.extent.width,
      swapchain.extent.height,
      1,
  };
  for (auto &frame : frame_data) {
    CreateFrameData(context, frame);
  }
}

void RenderGraph::Resize(VulkanContext &context, GLFWwindow *window) {
  int32_t width, height;
  glfwGetWindowSize(window, &width, &height);
  vkDeviceWaitIdle(context.device);
  DestroyVulkanSwapchain(context, swapchain);
  CreateVulkanSwapchain(context, width, height, swapchain);
  resize_requested = false;
}

void RenderGraph::Render(VulkanContext &context) {
  uint8_t frame_index = frame_number % FRAME_OVERLAP;
  auto &frame = frame_data[frame_index];

  VK_CHECK(vkWaitForFences(context.device, 1, &frame.render_fence, VK_TRUE,
                           std::numeric_limits<uint32_t>::max()));
  VK_CHECK(vkResetFences(context.device, 1, &frame.render_fence));

  uint32_t swapchain_image_index;

  {
    VkResult e = vkAcquireNextImageKHR(
        context.device, swapchain.obj, std::numeric_limits<uint32_t>::max(),
        frame.swapchain_semaphore, nullptr, &swapchain_image_index);
    if (e == VK_ERROR_OUT_OF_DATE_KHR) {
      resize_requested = true;
      return;
    }
  }

  VkCommandBuffer cmd = frame.command_buffer;

  VK_CHECK(
      vkResetCommandBuffer(cmd, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT));

  VkCommandBufferBeginInfo cmd_begin = vkinit::CommandBufferBeginInfo(
      VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

  VK_CHECK(vkBeginCommandBuffer(cmd, &cmd_begin));

  for (auto &render_pass_level : render_graph) {
    for (auto &render_pass : render_pass_level) {

      if (render_pass.condition != nullptr) {
        if (render_pass.condition == false) {
          continue;
        }
      }
      VkDependencyInfo dependency_info{};
      dependency_info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;

      dependency_info.pBufferMemoryBarriers =
          render_pass.dependency.buffer_deps.data();
      dependency_info.bufferMemoryBarrierCount =
          render_pass.dependency.buffer_deps.size();

      dependency_info.pImageMemoryBarriers =
          render_pass.dependency.image_deps.data();
      dependency_info.imageMemoryBarrierCount =
          render_pass.dependency.image_deps.size();

      dependency_info.pMemoryBarriers =
          render_pass.dependency.memory_deps.data();
      dependency_info.memoryBarrierCount =
          render_pass.dependency.memory_deps.size();

      vkCmdPipelineBarrier2(cmd, &dependency_info);

      render_pass.callback(cmd);
    }
  }

  VkDependencyInfo dependency_info{};
  dependency_info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;

  dependency_info.pBufferMemoryBarriers = root_dep.buffer_deps.data();
  dependency_info.bufferMemoryBarrierCount = root_dep.buffer_deps.size();

  dependency_info.pImageMemoryBarriers = root_dep.image_deps.data();
  dependency_info.imageMemoryBarrierCount = root_dep.image_deps.size();

  dependency_info.pMemoryBarriers = root_dep.memory_deps.data();
  dependency_info.memoryBarrierCount = root_dep.memory_deps.size();

  vkCmdPipelineBarrier2(cmd, &dependency_info);

  root_callback(cmd, swapchain.images[swapchain_image_index], swapchain.extent);

  VK_CHECK(vkEndCommandBuffer(cmd));

  VkCommandBufferSubmitInfo cmd_submit_info =
      vkinit::CommandBufferSubmitInfo(cmd);

  VkSemaphoreSubmitInfo wait_semaphore_info = vkinit::SemaphoreSubmitInfo(
      VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR,
      frame.swapchain_semaphore);

  VkSemaphoreSubmitInfo signal_semaphore_info = vkinit::SemaphoreSubmitInfo(
      VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, frame.render_semaphore);

  VkSubmitInfo2 submit_info = vkinit::SubmitInfo(
      &cmd_submit_info, &signal_semaphore_info, &wait_semaphore_info);

  {
    std::lock_guard<std::mutex> lock(context.graphics_queue_mutex);
    VK_CHECK(vkQueueSubmit2(context.graphics_queue, 1, &submit_info,
                            frame.render_fence));
  }

  VkPresentInfoKHR present_info{};
  present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  present_info.pSwapchains = &swapchain.obj;
  present_info.swapchainCount = 1;
  present_info.pWaitSemaphores = &frame.render_semaphore;
  present_info.waitSemaphoreCount = 1;
  present_info.pImageIndices = &swapchain_image_index;

  {
    std::lock_guard<std::mutex> lock(context.graphics_queue_mutex);
    VkResult e = vkQueuePresentKHR(context.graphics_queue, &present_info);
    if (e == VK_ERROR_OUT_OF_DATE_KHR) {
      resize_requested = true;
      return;
    }
  }

  frame_number++;
}

void RenderGraph::Destroy(VulkanContext &context) {
  DestroyVulkanSwapchain(context, swapchain);

  for (auto &frame : frame_data) {
    DestroyFrameData(context, frame);
  }
}
