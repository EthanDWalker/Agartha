#pragma once
#include "Backend/context.h"
#include <vulkan/vulkan.h>

struct AllocatedBuffer {
  VkBuffer buffer;
  VmaAllocation allocation;
  VmaAllocationInfo info;
};

void CreateBuffer(VulkanContext &context, size_t size, VkBufferUsageFlags usage,
                  VmaMemoryUsage memory_usage, AllocatedBuffer &buffer);

void DestroyBuffer(VulkanContext &context, AllocatedBuffer &buffer);
