#include "context.h"
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

namespace ui {
void Context::Create( GLFWwindow *window,
                     VkFormat *draw_format, VkFormat depth_format) {
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  io.ConfigFlags =
      ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_DockingEnable;

  ImGui_ImplGlfw_InitForVulkan(window, true);
  ImGui_ImplVulkan_InitInfo init_info = {};
  init_info.UseDynamicRendering = true;
  init_info.Instance = VulkanContext::instance;
  init_info.PhysicalDevice = VulkanContext::physical_device;
  init_info.Device = VulkanContext::device;
  init_info.QueueFamily = VulkanContext::graphics_queue_index;
  init_info.Queue = VulkanContext::graphics_queue;
  init_info.MinImageCount = 2;
  init_info.ImageCount = 3;
  init_info.DescriptorPoolSize = 100;
  init_info.PipelineInfoMain.PipelineRenderingCreateInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
  init_info.PipelineInfoMain.PipelineRenderingCreateInfo.colorAttachmentCount =
      1;
  init_info.PipelineInfoMain.PipelineRenderingCreateInfo
      .pColorAttachmentFormats = draw_format;
  init_info.PipelineInfoMain.PipelineRenderingCreateInfo.depthAttachmentFormat =
      depth_format;
  ImGui_ImplVulkan_Init(&init_info);
}

void Context::Destroy() {
  ImGui_ImplVulkan_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();
}
} // namespace ui
