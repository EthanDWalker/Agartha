#include "engine.h"
#include "Backend/context.h"
#include "Backend/frame_data.h"
#include "Backend/image.h"
#include "Backend/init.h"
#include "Backend/pipeline.h"
#include "Backend/swapchain.h"
#include "Backend/util.h"
#include "fmt/base.h"
#include <GLFW/glfw3.h>
#include <cstdint>
#include <limits>
#include <vulkan/vulkan_core.h>

void Engine::init() {
  glfwInit();
  glfwWindowHint(GLFW_CLIENT_API, GLFW_FALSE);
  window = glfwCreateWindow(1600, 900, "Engine", nullptr, nullptr);
  InitVulkanContext(window, DEBUG, context);
  CreateVulkanSwapchain(context, 1600, 900, swapchain);
  for (FrameData &frame : frame_data) {
    CreateFrameData(context, frame);
  }
  GraphicsPipelineBuilder graphics_builder;
  pipeline::SetShaders(context, "triangle.vert.spv", "triangle.frag.spv",
                       graphics_builder);
  pipeline::SetCullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE,
                        graphics_builder);
  pipeline::SetPolygonMode(VK_POLYGON_MODE_FILL, graphics_builder);
  pipeline::SetInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
                             graphics_builder);
  pipeline::SetNoBlending(graphics_builder);
  pipeline::SetNoDepthTest(graphics_builder);
  pipeline::SetNoMultisampling(graphics_builder);
  pipeline::BuildGraphicsPipeline(context, graphics_builder, triangle_pipeline);
}

void Engine::run() {
  uint8_t frame_index = 0;

  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
      glfwSetWindowShouldClose(window, true);
    }

    VK_CHECK(vkWaitForFences(context.device, 1,
                             &frame_data[frame_index].render_fence, VK_TRUE,
                             std::numeric_limits<uint32_t>::max()));
    VK_CHECK(vkResetFences(context.device, 1,
                           &frame_data[frame_index].render_fence));

    uint32_t swapchain_image_index;
    {
      VkResult e =
          vkAcquireNextImageKHR(context.device, swapchain.obj, 1000000000,
                                frame_data[frame_index].swapchain_semaphore,
                                nullptr, &swapchain_image_index);
      if (e == VK_ERROR_OUT_OF_DATE_KHR) {
        int32_t width, height;
        glfwGetWindowSize(window, &width, &height);
        vkDeviceWaitIdle(context.device);
        DestroyVulkanSwapchain(context, swapchain);
        CreateVulkanSwapchain(context, width, height, swapchain);
      }
    }

    VkCommandBuffer cmd = frame_data[frame_index].command_buffer;

    VK_CHECK(vkResetCommandBuffer(cmd, 0));

    VkCommandBufferBeginInfo cmd_begin_info = vkinit::CommandBufferBeginInfo(
        VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    VK_CHECK(vkBeginCommandBuffer(cmd, &cmd_begin_info));

    TransitionImage(cmd, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
                    swapchain.images[swapchain_image_index]);

    VkClearColorValue clear_value{};
    static float color;
    color += 0.005f;
    clear_value = {0.0f, 0.0f, std::abs(std::sin(color)), 1.0f};

    VkImageSubresourceRange clear_range =
        vkinit::ImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT);

    vkCmdClearColorImage(cmd, swapchain.images[swapchain_image_index],
                         VK_IMAGE_LAYOUT_GENERAL, &clear_value, 1,
                         &clear_range);

    TransitionImage(cmd, VK_IMAGE_LAYOUT_GENERAL,
                    VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                    swapchain.images[swapchain_image_index]);

    VK_CHECK(vkEndCommandBuffer(cmd));

    VkCommandBufferSubmitInfo cmd_info = vkinit::CommandBufferSubmitInfo(cmd);

    VkSemaphoreSubmitInfo wait_semaphore_info = vkinit::SemaphoreSubmitInfo(
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR,
        frame_data[frame_index].swapchain_semaphore);

    VkSemaphoreSubmitInfo signal_semaphore_info =
        vkinit::SemaphoreSubmitInfo(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
                                    frame_data[frame_index].render_semaphore);

    VkSubmitInfo2 submit_info = vkinit::SubmitInfo(
        &cmd_info, &signal_semaphore_info, &wait_semaphore_info);

    VK_CHECK(vkQueueSubmit2(context.graphics_queue, 1, &submit_info,
                            frame_data[frame_index].render_fence));

    VkPresentInfoKHR present_info{};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.pSwapchains = &swapchain.obj;
    present_info.swapchainCount = 1;
    present_info.pWaitSemaphores = &frame_data[frame_index].render_semaphore;
    present_info.waitSemaphoreCount = 1;
    present_info.pImageIndices = &swapchain_image_index;

    {
      VkResult e = vkQueuePresentKHR(context.graphics_queue, &present_info);
      if (e == VK_ERROR_OUT_OF_DATE_KHR) {
        int32_t width, height;
        fmt::println("{}", static_cast<int>(e));
        glfwGetWindowSize(window, &width, &height);
        vkDeviceWaitIdle(context.device);
        DestroyVulkanSwapchain(context, swapchain);
        CreateVulkanSwapchain(context, width, height, swapchain);
      }
    }

    frame_index ^= 1;
  }
}

void Engine::destroy() {
  vkDeviceWaitIdle(context.device);

  for (FrameData &frame : frame_data) {
    DestroyFrameData(context, frame);
  }

  pipeline::DestroyPipeline(context, triangle_pipeline);

  DestroyVulkanSwapchain(context, swapchain);
  DestroyVulkanContext(context);

  glfwDestroyWindow(window);
  glfwTerminate();
}
