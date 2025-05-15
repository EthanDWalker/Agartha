#include "buffer.h"
#include "Backend/context.h"
#include "Backend/util.h"

void CreateBuffer(VulkanContext &context, size_t size, VkBufferUsageFlags usage,
                  VmaMemoryUsage memory_usage, AllocatedBuffer &buffer) {
  VkBufferCreateInfo buffer_ci{};
  buffer_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  buffer_ci.size = size;
  buffer_ci.usage = usage;

  VmaAllocationCreateInfo alloc_ci{};
  alloc_ci.usage = memory_usage;
  alloc_ci.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

  VK_CHECK(vmaCreateBuffer(context.allocator, &buffer_ci, &alloc_ci,
                           &buffer.buffer, &buffer.allocation, &buffer.info));
}

void DestroyBuffer(VulkanContext &context, AllocatedBuffer &buffer) {
  vmaDestroyBuffer(context.allocator, buffer.buffer, buffer.allocation);
}
