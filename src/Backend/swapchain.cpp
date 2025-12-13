#include "swapchain.h"
#include "context.h"
#include <VkBootstrap.h>
#include <cstdint>

void CreateVulkanSwapchain(uint32_t width, uint32_t height, Swapchain &swapchain) {
  vkb::SwapchainBuilder swapchain_builder{
      VulkanContext::physical_device,
      VulkanContext::device,
      VulkanContext::surface,
  };

  swapchain.format = VK_FORMAT_B8G8R8A8_UNORM;

  vkb::Swapchain vkb_swapchain =
      swapchain_builder
          .set_desired_format(VkSurfaceFormatKHR{.format = swapchain.format,
                                                 .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR})
          .set_desired_present_mode(VK_PRESENT_MODE_MAILBOX_KHR)
          .set_desired_extent(width, height)
          .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
          .build()
          .value();

  swapchain.extent = vkb_swapchain.extent;
  swapchain.obj = vkb_swapchain.swapchain;
  swapchain.images = vkb_swapchain.get_images().value();
  swapchain.image_views = vkb_swapchain.get_image_views().value();
}

void DestroyVulkanSwapchain(Swapchain &swapchain) {
  vkDestroySwapchainKHR(VulkanContext::device, swapchain.obj, nullptr);
  for (VkImageView image_view : swapchain.image_views) {
    vkDestroyImageView(VulkanContext::device, image_view, nullptr);
  }
}
