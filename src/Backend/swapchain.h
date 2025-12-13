#pragma once
#include <volk.h>
#include <vector>

struct Swapchain {
  std::vector<VkImage> images;
  std::vector<VkImageView> image_views;
  VkSwapchainKHR obj;
  VkFormat format;
  VkExtent2D extent;
};

void CreateVulkanSwapchain(uint32_t width, uint32_t height, Swapchain &swapchain);

void DestroyVulkanSwapchain(Swapchain &swapchain);
