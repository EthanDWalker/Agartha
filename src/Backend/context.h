#pragma once
#include <volk.h>

#include <GLFW/glfw3.h>
#include <cstdint>
#include <mutex>
#include <vma/vk_mem_alloc.h>

struct VulkanContext {
  static std::mutex graphics_queue_mutex;
  static std::mutex compute_queue_mutex;
  static VkInstance instance;
  static VkDevice device;
  static VkPhysicalDevice physical_device;
  static VkSurfaceKHR surface;
  static VmaAllocator allocator;
  static VkDebugUtilsMessengerEXT debug_messenger;
  static VkQueue graphics_queue;
  static VkQueue compute_queue;
  static uint32_t graphics_queue_index;
  static uint32_t compute_queue_index;

  static void Init(GLFWwindow *window);
  static void Destroy();
};
