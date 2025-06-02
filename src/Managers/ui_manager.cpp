#include "ui_manager.h"
#include <imgui.h>
#include <imgui_impl_vulkan.h>

void UiManager::Create(VulkanContext &context, VkFormat color_format,
                       VkFormat depth_format, GLFWwindow *window) {
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  ImGui_ImplGlfw_InitForVulkan(window, true);
  ImGui_ImplVulkan_InitInfo init_info{};
  init_info.Instance = context.instance;
  init_info.PhysicalDevice = context.physical_device;
  init_info.Device = context.device;
  init_info.QueueFamily = context.graphics_queue_index;
  init_info.Queue = context.graphics_queue;
  init_info.DescriptorPoolSize = 10;
  init_info.UseDynamicRendering = true;
  init_info.ApiVersion = VK_API_VERSION_1_3;
  init_info.MinImageCount = 3;
  init_info.ImageCount = 3;
  init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

  init_info.PipelineRenderingCreateInfo = {};
  init_info.PipelineRenderingCreateInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
  init_info.PipelineRenderingCreateInfo.pColorAttachmentFormats = &color_format;
  init_info.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
  init_info.PipelineRenderingCreateInfo.depthAttachmentFormat = depth_format;

  ImGui_ImplVulkan_Init(&init_info);
}

void UiManager::Render(VkCommandBuffer cmd) {
  static float fortnite = 0.0f;
  ImGui_ImplVulkan_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();
  if (ImGui::Begin("background")) {
    ImGui::InputFloat("for", &fortnite);
  }
  ImGui::End();

  ImGui::Render();

  ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
}

void UiManager::Destroy() {
  ImGui_ImplVulkan_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();
}
