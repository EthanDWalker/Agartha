#pragma once
#include "Backend/context.h"
#include <volk.h>

struct AllocatedBuffer {
  VmaAllocationInfo info;
  VkBuffer buffer;
  VmaAllocation allocation;
};

void CreateBuffer(size_t size, VkBufferUsageFlags usage, VmaMemoryUsage memory_usage,
                  AllocatedBuffer &buffer);

void CreateBufferData(void *data, size_t size, VkBufferUsageFlags usage, AllocatedBuffer &buffer);

void CreateBufferDataAsync(void *data, size_t size, VkBufferUsageFlags usage,
                           AllocatedBuffer &buffer);

void UpdateBuffer(void *data, size_t size, size_t offset, AllocatedBuffer &buffer);

void UpdateBufferAsync(void *data, size_t size, size_t offset, AllocatedBuffer &buffer);

void DestroyBuffer(AllocatedBuffer &buffer);
