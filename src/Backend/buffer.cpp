#include "buffer.h"
#include "Backend/context.h"
#include "Backend/immediate_submit.h"
#include "Backend/util.h"

#define VMA_IMPLEMENTATION
#include <vma/vk_mem_alloc.h>

void CreateBuffer(size_t size, VkBufferUsageFlags usage, VmaMemoryUsage memory_usage,
                  AllocatedBuffer &buffer) {
  VkBufferCreateInfo buffer_ci{};
  buffer_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  buffer_ci.size = size;
  buffer_ci.usage = usage;

  VmaAllocationCreateInfo alloc_ci{};
  alloc_ci.usage = memory_usage;
  alloc_ci.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
  if (memory_usage == VMA_MEMORY_USAGE_AUTO || memory_usage == VMA_MEMORY_USAGE_AUTO_PREFER_HOST) {
    alloc_ci.flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
  }

  VK_CHECK(vmaCreateBuffer(VulkanContext::allocator, &buffer_ci, &alloc_ci, &buffer.buffer,
                           &buffer.allocation, &buffer.info));
}

void CreateBufferData(void *data, size_t size, VkBufferUsageFlags usage, AllocatedBuffer &buffer) {
  AllocatedBuffer upload_buffer;
  CreateBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, upload_buffer);

  memcpy(upload_buffer.allocation->GetMappedData(), data, size);

  CreateBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | usage,
               VMA_MEMORY_USAGE_GPU_ONLY, buffer);

  ImmediateSubmit::Submit([&](VkCommandBuffer cmd) {
    VkBufferCopy buffer_copy{};
    buffer_copy.size = size;
    buffer_copy.dstOffset = 0;
    buffer_copy.srcOffset = 0;

    vkCmdCopyBuffer(cmd, upload_buffer.buffer, buffer.buffer, 1, &buffer_copy);
  });

  DestroyBuffer(upload_buffer);
}

void CreateBufferDataAsync(void *data, size_t size, VkBufferUsageFlags usage,
                           AllocatedBuffer &buffer) {
  AllocatedBuffer upload_buffer;
  CreateBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, upload_buffer);

  memcpy(upload_buffer.allocation->GetMappedData(), data, size);

  CreateBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | usage,
               VMA_MEMORY_USAGE_GPU_ONLY, buffer);

  ImmediateSubmit::Submit([&](VkCommandBuffer cmd) {
    VkBufferCopy buffer_copy{};
    buffer_copy.size = size;
    buffer_copy.dstOffset = 0;
    buffer_copy.srcOffset = 0;

    vkCmdCopyBuffer(cmd, upload_buffer.buffer, buffer.buffer, 1, &buffer_copy);
  });

  DestroyBuffer(upload_buffer);
}

// Size is the size of the new data not the entire buffer
void UpdateBuffer(void *data, size_t size, size_t offset,
                  AllocatedBuffer &buffer) {
  AllocatedBuffer upload_buffer;
  CreateBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, upload_buffer);

  memcpy(upload_buffer.info.pMappedData, data, size);

  ImmediateSubmit::Submit([&](VkCommandBuffer cmd) {
    VkBufferCopy buffer_copy{};
    buffer_copy.size = size;
    buffer_copy.dstOffset = offset;
    buffer_copy.srcOffset = 0;

    vkCmdCopyBuffer(cmd, upload_buffer.buffer, buffer.buffer, 1, &buffer_copy);
  });

  DestroyBuffer(upload_buffer);
}

void UpdateBufferAsync(void *data, size_t size, size_t offset, AllocatedBuffer &buffer) {
  AllocatedBuffer upload_buffer;
  CreateBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, upload_buffer);

  memcpy(upload_buffer.info.pMappedData, data, size);

  ImmediateSubmit::Submit([&](VkCommandBuffer cmd) {
    VkBufferCopy buffer_copy{};
    buffer_copy.size = size;
    buffer_copy.dstOffset = offset;
    buffer_copy.srcOffset = 0;

    vkCmdCopyBuffer(cmd, upload_buffer.buffer, buffer.buffer, 1, &buffer_copy);
  });

  DestroyBuffer(upload_buffer);
}

void DestroyBuffer(AllocatedBuffer &buffer) {
  vmaDestroyBuffer(VulkanContext::allocator, buffer.buffer, buffer.allocation);
}
