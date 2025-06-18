#pragma once
#include <cstdint>
#include <mutex>
#include <Volk/volk.h>
#include <GLFW/glfw3.h>
#include <vma/vk_mem_alloc.h>

struct VulkanContext {
  std::mutex graphics_queue_mutex;
  std::mutex compute_queue_mutex;
  VkInstance instance;
  VkDevice device;
  VkPhysicalDevice physical_device;
  VkSurfaceKHR surface;
  VmaAllocator allocator;
  VkDebugUtilsMessengerEXT debug_messenger;
  VkQueue graphics_queue;
  VkQueue compute_queue;
  uint32_t graphics_queue_index;
  uint32_t compute_queue_index;
};

void InitVulkanContext(GLFWwindow *window, bool debug, VulkanContext &context);

void DestroyVulkanContext(VulkanContext &context);
