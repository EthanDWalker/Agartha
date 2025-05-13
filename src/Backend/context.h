#pragma once
#include <cstdint>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vma/vk_mem_alloc.h>

struct VulkanContext {
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
