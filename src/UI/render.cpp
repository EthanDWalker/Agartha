#include "render.h"

#include "Backend/allocated_image.h"
#include "Backend/init.h"
#include <array>
#include <glm/vec3.hpp>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

void UpdateUi(GLFWwindow *window, glm::vec3 *directional_light) {
  ImGui_ImplVulkan_NewFrame();
  ImGui_ImplGlfw_NewFrame();

  ImGui::NewFrame();
  if (ImGui::Begin("Lighting")) {
    ImGui::DragFloat3("Sun", (float *)directional_light, 0.1f, -4.0f, 4.0f);
  }
  ImGui::End();
  ImGui::EndFrame();
}

void RenderUi(VkCommandBuffer cmd, AllocatedImage &draw_image) {
  VkViewport viewport = vkinit::Viewport(draw_image.extent);
  vkCmdSetViewport(cmd, 0, 1, &viewport);
  VkRect2D scissor = vkinit::Scissor(draw_image.extent);
  vkCmdSetScissor(cmd, 0, 1, &scissor);

  VkRenderingAttachmentInfo color_att =
      vkinit::AttachmentInfo(draw_image.image_view, nullptr, nullptr,
                             VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

  std::array<VkRenderingAttachmentInfo, 1> attachments = {
      color_att,
  };

  VkRenderingInfo rendering_info =
      vkinit::RenderingInfo(draw_image.extent, attachments, nullptr);

  vkCmdBeginRendering(cmd, &rendering_info);

  ImGui::Render();
  ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

  vkCmdEndRendering(cmd);
}
