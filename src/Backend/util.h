#pragma once
#include "context.h"
#include <assert.h>
#include <fmt/core.h>
#include <vulkan/vk_enum_string_helper.h>

#define VK_CHECK(x)                                                            \
  do {                                                                         \
    VkResult err = x;                                                          \
    if (err < 0) {                                                             \
      fmt::println("Detected Vulkan error: {}", string_VkResult(err));         \
      assert(err >= 0);                                                        \
    }                                                                          \
  } while (0)

VkDeviceAddress GetDeviceAddress(VulkanContext &context, VkBuffer buffer);

VkDeviceAddress GetDeviceAddress(VulkanContext &context,
                                 VkAccelerationStructureKHR as);
