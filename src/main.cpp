#include "Backend/context.h"
#include "Backend/swapchain.h"
#include <GLFW/glfw3.h>
#include <cstdint>

int main() {
  VulkanContext context;
  Swapchain swapchain;
  constexpr uint32_t width = 1600;
  constexpr uint32_t height = 900;

  glfwInit();
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

  GLFWwindow *window =
      glfwCreateWindow(width, height, "Engine", nullptr, nullptr);

  InitVulkanContext(window, true, context);
  CreateVulkanSwapchain(context, width, height, swapchain);

  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
      glfwSetWindowShouldClose(window, true);
    }
  }

  DestroyVulkanSwapchain(context, swapchain);
  DestroyVulkanContext(context);

  glfwDestroyWindow(window);
  glfwTerminate();
}
