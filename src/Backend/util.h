#pragma once
#include <assert.h>
#include <fmt/core.h>
#include <glm/mat4x4.hpp>
#define VK_NO_PROTOTYPES
#include <vulkan/vk_enum_string_helper.h>


#define VK_CHECK(x)                                                                                \
  do {                                                                                             \
    VkResult err = x;                                                                              \
    if (err < 0) {                                                                                 \
      fmt::println("Detected Vulkan error: {}", string_VkResult(err));                             \
      assert(false);                                                                               \
    }                                                                                              \
  } while (0)

VkDeviceAddress GetDeviceAddress(VkBuffer buffer);

VkDeviceAddress GetDeviceAddress(VkAccelerationStructureKHR as);

uint32_t AlignedSize(uint32_t value, uint32_t alignment);

size_t AlignedSize(size_t value, size_t alignment);

VkTransformMatrixKHR Mat4ToVkTransform(glm::mat4 &m);
