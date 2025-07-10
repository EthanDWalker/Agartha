#include "render.h"

#include <glm/vec3.hpp>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

bool UpdateUi(GLFWwindow *window, glm::vec3 *directional_light) {
  ImGui_ImplVulkan_NewFrame();
  ImGui_ImplGlfw_NewFrame();

  ImGui::NewFrame();
  if (ImGui::Begin("Lighting")) {
    ImGui::DragFloat3("Sun", (float *)directional_light, 0.1f, -4.0f, 4.0f);
  }
  ImGui::End();
  ImGui::EndFrame();
  return ImGui::GetIO().WantCaptureMouse;
}

void RenderUi(VkCommandBuffer cmd) {
  ImGui::Render();
  ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
}
