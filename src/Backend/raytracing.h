#pragma once

#include "buffer.h"
#include "context.h"
#include "types.h"

struct AccelerationStructure {
  AllocatedBuffer buffer;
  VkAccelerationStructureKHR obj;
};

// AccelerationStructure
struct ASBuilder {
  VkAccelerationStructureBuildRangeInfoKHR offset{};
  VkAccelerationStructureGeometryKHR geomertry{};

  void SetMesh(VulkanContext &context, Mesh &mesh,
               VkDeviceAddress index_address);

  void SetInstances(VulkanContext &context, AllocatedBuffer instance_buffer);

  AccelerationStructure
  CreateBottomLevelAS(VulkanContext &context,
                      VkBuildAccelerationStructureFlagsKHR flags);

  AccelerationStructure
  CreateTopLevelAS(VulkanContext &context,
                   VkBuildAccelerationStructureFlagsKHR flags);
};

struct RaytracingPipelineBuilder {};

void DestroyAccelerationStructure(VulkanContext &context,
                                  AccelerationStructure &as);
