#include "buffer.h"
#include "Backend/context.h"
#include "Backend/immediate_submit.h"
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

void CreateBufferData(VulkanContext &context, ImmediateSubmit immediate_submit,
                      void *data, size_t size, VkBufferUsageFlags usage,
                      AllocatedBuffer &buffer) {
  AllocatedBuffer upload_buffer;
  CreateBuffer(context, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
               VMA_MEMORY_USAGE_CPU_TO_GPU, upload_buffer);

  memcpy(upload_buffer.info.pMappedData, data, size);

  CreateBuffer(context, size,
               VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                   VK_BUFFER_USAGE_TRANSFER_DST_BIT | usage,
               VMA_MEMORY_USAGE_GPU_ONLY, buffer);

  immediate_submit.Submit(context, [&](VkCommandBuffer cmd) {
    VkBufferCopy buffer_copy{};
    buffer_copy.size = size;
    buffer_copy.dstOffset = 0;
    buffer_copy.srcOffset = 0;

    vkCmdCopyBuffer(cmd, upload_buffer.buffer, buffer.buffer, 1, &buffer_copy);
  });

  DestroyBuffer(context, upload_buffer);
}

void DestroyBuffer(VulkanContext &context, AllocatedBuffer &buffer) {
  vmaDestroyBuffer(context.allocator, buffer.buffer, buffer.allocation);
}
