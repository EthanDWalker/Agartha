#pragma once
#include "Backend/context.h"
#include "Backend/immediate_submit.h"
#include <volk.h>

struct AllocatedBuffer {
  VmaAllocationInfo info;
  VkBuffer buffer;
  VmaAllocation allocation;
};

void CreateBuffer(VulkanContext &context, size_t size, VkBufferUsageFlags usage,
                  VmaMemoryUsage memory_usage, AllocatedBuffer &buffer);

void CreateBufferData(VulkanContext &context, ImmediateSubmit immediate_submit,
                      void *data, size_t size, VkBufferUsageFlags usage,
                      AllocatedBuffer &buffer);

void CreateBufferDataAsync(VulkanContext &context, void *data, size_t size,
                           VkBufferUsageFlags usage, AllocatedBuffer &buffer);

void UpdateBuffer(VulkanContext &context, ImmediateSubmit &immediate_submit,
                  void *data, size_t size, size_t offset,
                  AllocatedBuffer &buffer);

void UpdateBufferAsync(VulkanContext &context, void *data, size_t size,
                       size_t offset, AllocatedBuffer &buffer);

void DestroyBuffer(VulkanContext &context, AllocatedBuffer &buffer);
