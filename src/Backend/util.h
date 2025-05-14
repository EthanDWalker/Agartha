#pragma once
#include <assert.h>
#include <fmt/core.h>
#include <vulkan/vk_enum_string_helper.h>
#include <vulkan/vulkan.h>

#define VK_CHECK(x)                                                            \
  do {                                                                         \
    VkResult err = x;                                                          \
    if (err < 0) {                                                             \
      fmt::println("Detected Vulkan error: {}", string_VkResult(err));         \
      assert(err >= 0);                                                        \
    }                                                                          \
  } while (0)
