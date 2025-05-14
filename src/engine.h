#pragma once

#include "Backend/context.h"
#include "Backend/frame_data.h"
#include "Backend/swapchain.h"
#include "GLFW/glfw3.h"
#include <cstdint>

#if defined(DEBUG)
constexpr bool DEBUG = true;
#else
constexpr bool DEBUG = false;
#endif

constexpr uint8_t FRAME_OVERLAP = 2;

struct Engine {
  VulkanContext context;
  Swapchain swapchain;
  FrameData frame_data[FRAME_OVERLAP];
  GLFWwindow *window;

  void init();

  void run();

  void destroy();
};
