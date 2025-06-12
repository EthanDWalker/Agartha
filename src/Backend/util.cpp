#include "util.h"

VkDeviceAddress GetDeviceAddress(VulkanContext &context,
                                 AllocatedBuffer &buffer) {
  VkBufferDeviceAddressInfo device_address_info{};
  device_address_info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
  device_address_info.buffer = buffer.buffer;
  return vkGetBufferDeviceAddress(context.device, &device_address_info);
}
