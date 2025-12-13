#include "util.h"
#include "Backend/context.h"
#include <glm/mat3x4.hpp>
#include <glm/mat4x4.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>

VkDeviceAddress GetDeviceAddress(VkBuffer buffer) {
  VkBufferDeviceAddressInfo device_address_info{};
  device_address_info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
  device_address_info.buffer = buffer;
  return vkGetBufferDeviceAddress(VulkanContext::device, &device_address_info);
}

VkDeviceAddress GetDeviceAddress(VkAccelerationStructureKHR as) {
  VkAccelerationStructureDeviceAddressInfoKHR device_address_info{};
  device_address_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
  device_address_info.accelerationStructure = as;
  return vkGetAccelerationStructureDeviceAddressKHR(VulkanContext::device, &device_address_info);
}

uint32_t AlignedSize(uint32_t value, uint32_t alignment) {
  return (value + alignment - 1) & ~(alignment - 1);
}

size_t AlignedSize(size_t value, size_t alignment) {
  return (value + alignment - 1) & ~(alignment - 1);
}

VkTransformMatrixKHR Mat4ToVkTransform(glm::mat4 &m) {
  VkTransformMatrixKHR transform;

  for (uint8_t i = 0; i < 3; i++) {
    for (uint8_t j = 0; j < 4; j++) {
      transform.matrix[i][j] = m[j][i];
    }
  }

  return transform;
}
