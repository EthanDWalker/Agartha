#pragma once

#include "buffer.h"
#include "context.h"
#include "types.h"

struct AccelerationStructure {
  AllocatedBuffer buffer;
  VkAccelerationStructureKHR obj;
};

void CreateBottomLevelAS(VulkanContext &context, Mesh &mesh,
                         VkDeviceAddress index_address,
                         VkBuildAccelerationStructureFlagsKHR flags,
                         AccelerationStructure &as);

void CreateTopLevelAS(VulkanContext &context, VkDeviceAddress instance_address,
                      size_t instance_count,
                      VkBuildAccelerationStructureFlagsKHR flags,
                      AccelerationStructure &as);

void DestroyAccelerationStructure(VulkanContext &context,
                                  AccelerationStructure &as);
