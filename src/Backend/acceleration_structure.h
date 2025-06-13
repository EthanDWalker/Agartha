#pragma once

#include "buffer.h"
#include "context.h"
#include "types.h"
#include <span>

struct AccelerationStructure {
  AllocatedBuffer buffer;
  VkAccelerationStructureKHR obj;
};

// AccelerationStructure
struct ASBuilder {
  VkAccelerationStructureBuildRangeInfoKHR offset{};
  VkAccelerationStructureGeometryKHR geomertry{};
  AllocatedBuffer instance_buffer{VK_NULL_HANDLE};

  void SetMesh(VulkanContext &context, Mesh &mesh,
               VkDeviceAddress index_address);

  void SetInstances(VulkanContext &context, std::span<Instance> instances,
                    AccelerationStructure bottom_level_as);

  AccelerationStructure
  CreateBottomLevelAS(VulkanContext &context,
                      VkBuildAccelerationStructureFlagsKHR flags);

  AccelerationStructure
  CreateTopLevelAS(VulkanContext &context,
                   VkBuildAccelerationStructureFlagsKHR flags);
};

void DestroyAccelerationStructure(VulkanContext &context,
                                  AccelerationStructure &as);
