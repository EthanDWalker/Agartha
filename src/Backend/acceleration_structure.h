#pragma once

#include "buffer.h"
#include "types.h"

struct AccelerationStructure {
  AllocatedBuffer buffer;
  VkAccelerationStructureKHR obj;
};

void CreateBottomLevelAS(Mesh &mesh, VkDeviceAddress index_address,
                         VkBuildAccelerationStructureFlagsKHR flags, AccelerationStructure &as);

void CreateTopLevelAS(VkDeviceAddress instance_address, size_t instance_count,
                      VkBuildAccelerationStructureFlagsKHR flags, AccelerationStructure &as);

void DestroyAccelerationStructure(AccelerationStructure &as);
