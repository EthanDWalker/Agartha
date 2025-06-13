#include "util.h"

VkDeviceAddress GetDeviceAddress(VulkanContext &context, VkBuffer buffer) {
  VkBufferDeviceAddressInfo device_address_info{};
  device_address_info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
  device_address_info.buffer = buffer;
  return vkGetBufferDeviceAddress(context.device, &device_address_info);
}

VkDeviceAddress GetDeviceAddress(VulkanContext &context,
                                 VkAccelerationStructureKHR as) {
  VkAccelerationStructureDeviceAddressInfoKHR device_address_info{};
  device_address_info.sType =
      VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
  device_address_info.accelerationStructure = as;
  return vkGetAccelerationStructureDeviceAddressKHR(context.device,
                                                    &device_address_info);
}

uint32_t AlignedSize(uint32_t value, uint32_t alignment) {
  return (value + alignment - 1) & ~(alignment - 1);
}

size_t AlignedSize(size_t value, size_t alignment) {
  return (value + alignment - 1) & ~(alignment - 1);
}
