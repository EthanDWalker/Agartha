#pragma once

#include "Backend/context.h"
#include <GLFW/glfw3.h>
#include <imgui_impl_glfw.h>
#include <Volk/volk.h>
#include <imgui_impl_vulkan.h>

struct UiManager {
  void Create(VulkanContext &context, VkFormat color_format,
              VkFormat depth_format, GLFWwindow *window);

  void Render(VkCommandBuffer cmd);

  void Destroy();
};
