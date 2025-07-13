#include "context.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

void UiContext::Create(VulkanContext &vulkan_context, GLFWwindow *window,
                       VkFormat *draw_format) {
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  io.ConfigFlags = ImGuiConfigFlags_NavEnableKeyboard;

  ImGui_ImplGlfw_InitForVulkan(window, true);
  ImGui_ImplVulkan_InitInfo init_info = {};
  init_info.UseDynamicRendering = true;
  init_info.Instance = vulkan_context.instance;
  init_info.PhysicalDevice = vulkan_context.physical_device;
  init_info.Device = vulkan_context.device;
  init_info.QueueFamily = vulkan_context.graphics_queue_index;
  init_info.Queue = vulkan_context.graphics_queue;
  init_info.MinImageCount = 2;
  init_info.ImageCount = 3;
  init_info.DescriptorPoolSize = 100;
  init_info.PipelineRenderingCreateInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
  init_info.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
  init_info.PipelineRenderingCreateInfo.pColorAttachmentFormats = draw_format;
  ImGui_ImplVulkan_Init(&init_info);
}

bool UiContext::Update() {
  ImGui_ImplVulkan_NewFrame();
  ImGui_ImplGlfw_NewFrame();

  ImGui::NewFrame();
  for (auto &panel : panels) {
    panel();
  }
  ImGui::EndFrame();
  return ImGui::GetIO().WantCaptureMouse;
}

void UiContext::Render(VkCommandBuffer cmd) {
  ImGui::Render();
  ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
}

void UiContext::Destroy() {
  ImGui_ImplVulkan_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();
}
