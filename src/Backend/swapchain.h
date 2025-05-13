#pragma once
#include "Backend/context.h"
#include <vector>
#include <vulkan/vulkan.h>

struct Swapchain {
  std::vector<VkImage> images;
  std::vector<VkImageView> image_views;
  VkSwapchainKHR obj;
  VkFormat format;
  VkExtent2D extent;
};

void CreateVulkanSwapchain(VulkanContext &vulkan_context, uint32_t width,
                           uint32_t height, Swapchain &swapchain);

void DestroyVulkanSwapchain(VulkanContext &vulkan_context,
                            Swapchain &swapchain);
