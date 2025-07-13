#pragma once

#include "Backend/context.h"
#include <functional>

struct UiContext {
  std::vector<std::function<void()>> panels;

  void Create(VulkanContext &vulkan_context, GLFWwindow *window,
              VkFormat *draw_format);

  bool Update();

  void Render(VkCommandBuffer cmd);

  void Destroy();
};
