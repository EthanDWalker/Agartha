#include "render_graph.h"
#include "Backend/allocated_image.h"
#include "Backend/context.h"
#include "Backend/frame_data.h"
#include "Backend/init.h"
#include "Backend/swapchain.h"
#include "Backend/util.h"
#include "GLFW/glfw3.h"
#include <cstdint>
#include <limits>

void RenderGraph::Init(VulkanContext &context, GLFWwindow *window) {
  int32_t width, height;
  glfwGetWindowSize(window, &width, &height);
  CreateVulkanSwapchain(context, width, height, swapchain);
  VkExtent3D draw_image_extent = {
      swapchain.extent.width,
      swapchain.extent.height,
      1,
  };
  CreateAllocatedImage(
      context, draw_image_extent, VK_FORMAT_R16G16B16A16_SFLOAT,
      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
          VK_IMAGE_USAGE_STORAGE_BIT,
      draw_image);
  CreateAllocatedImage(context, draw_image_extent, VK_FORMAT_D32_SFLOAT,
                       VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                       depth_image);
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

  TransitionImage(cmd, VK_IMAGE_LAYOUT_UNDEFINED,
                  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, draw_image.image);

  TransitionImage(cmd, VK_IMAGE_LAYOUT_UNDEFINED,
                  VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, depth_image.image);

  VkClearColorValue clear_color_value{};
  clear_color_value = {0.0f, 0.0f, 0.0f, 0.0f};

  VkClearValue clear_value{};
  clear_value.color = clear_color_value;

  VkViewport viewport = {};
  viewport.x = 0;
  viewport.y = 0;
  viewport.width = draw_image.extent.width;
  viewport.height = draw_image.extent.height;
  viewport.minDepth = 1.0f;
  viewport.maxDepth = 0.0f;

  vkCmdSetViewport(cmd, 0, 1, &viewport);

  VkRect2D scissor = {};
  scissor.offset.x = 0;
  scissor.offset.y = 0;
  scissor.extent.width = draw_image.extent.width;
  scissor.extent.height = draw_image.extent.height;

  vkCmdSetScissor(cmd, 0, 1, &scissor);

  // DO RENDERING



  // root node work

  TransitionImage(cmd, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                  VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, draw_image.image);

  TransitionImage(cmd, VK_IMAGE_LAYOUT_UNDEFINED,
                  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                  swapchain.images[swapchain_image_index]);

  CopyImageToImage(
      cmd, draw_image.image, swapchain.images[swapchain_image_index],
      {draw_image.extent.width, draw_image.extent.height}, swapchain.extent);

  TransitionImage(cmd, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                  VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                  swapchain.images[swapchain_image_index]);

  // root node work

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

  VK_CHECK(vkQueueSubmit2(context.graphics_queue, 1, &submit_info,
                          frame.render_fence));

  VkPresentInfoKHR present_info{};
  present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  present_info.pSwapchains = &swapchain.obj;
  present_info.swapchainCount = 1;
  present_info.pWaitSemaphores = &frame.render_semaphore;
  present_info.waitSemaphoreCount = 1;
  present_info.pImageIndices = &swapchain_image_index;

  {
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
  DestroyAllocatedImage(context, draw_image);
  DestroyAllocatedImage(context, depth_image);

  for (auto &frame : frame_data) {
    DestroyFrameData(context, frame);
  }
}
