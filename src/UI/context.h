#pragma once

#include "Backend/context.h"

namespace ui {
struct Context {
  void Create(GLFWwindow *window, VkFormat *draw_format, VkFormat depth_format);

  void Destroy();
};
} // namespace ui
