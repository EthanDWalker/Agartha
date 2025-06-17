#include "acceleration_structure.h"
#include "buffer.h"
#include "context.h"
#include "immediate_submit.h"
#include "util.h"
#include <vector>

void CreateBottomLevelAS(VulkanContext &context, Mesh &mesh,
                         VkDeviceAddress index_address,
                         VkBuildAccelerationStructureFlagsKHR flags,
                         AccelerationStructure &as) {
  VkDeviceAddress vertex_address =
      GetDeviceAddress(context, mesh.vertex_buffer.buffer);

  VkAccelerationStructureGeometryTrianglesDataKHR triangles{};
  triangles.sType =
      VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
  triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
  triangles.vertexData.deviceAddress = vertex_address;
  triangles.vertexStride = sizeof(Vertex);
  triangles.indexType = VK_INDEX_TYPE_UINT32;
  triangles.indexData.deviceAddress = index_address;
  triangles.maxVertex = (mesh.vertex_buffer.info.size / sizeof(Vertex)) - 1;

  VkAccelerationStructureGeometryKHR geometry{};
  geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
  geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
  geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
  geometry.geometry.triangles = triangles;

  VkAccelerationStructureBuildRangeInfoKHR offset{};
  offset.primitiveCount = mesh.index_count / 3;
  offset.primitiveOffset = mesh.first_index * sizeof(uint32_t);

  VkAccelerationStructureBuildGeometryInfoKHR build_info{};
  build_info.sType =
      VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
  build_info.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
  build_info.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
  build_info.flags = flags;
  build_info.geometryCount = 1;
  build_info.pGeometries = &geometry;

  VkAccelerationStructureBuildSizesInfoKHR size_info{};
  size_info.sType =
      VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;

  vkGetAccelerationStructureBuildSizesKHR(
      context.device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
      &build_info, &offset.primitiveCount, &size_info);

  CreateBuffer(context, size_info.accelerationStructureSize,
               VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                   VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR,
               VMA_MEMORY_USAGE_GPU_ONLY, as.buffer);

  VkAccelerationStructureCreateInfoKHR as_ci{};
  as_ci.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
  as_ci.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
  as_ci.size = size_info.accelerationStructureSize;
  as_ci.buffer = as.buffer.buffer;

  vkCreateAccelerationStructureKHR(context.device, &as_ci, nullptr, &as.obj);

  AllocatedBuffer scratch_buffer{};
  CreateBuffer(context, size_info.buildScratchSize,
               VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
               VMA_MEMORY_USAGE_GPU_ONLY, scratch_buffer);

  VkDeviceAddress scratch_address =
      GetDeviceAddress(context, scratch_buffer.buffer);

  build_info.dstAccelerationStructure = as.obj;
  build_info.scratchData.deviceAddress = scratch_address;

  std::vector<VkAccelerationStructureBuildRangeInfoKHR *> range_info = {
      &offset,
  };

  ImmediateSubmit::SubmitAsync(context, [&](VkCommandBuffer cmd) {
    vkCmdBuildAccelerationStructuresKHR(cmd, 1, &build_info, range_info.data());
  });

  DestroyBuffer(context, scratch_buffer);
}

void CreateTopLevelAS(VulkanContext &context, VkDeviceAddress instance_address,
                      size_t instance_count,
                      VkBuildAccelerationStructureFlagsKHR flags,
                      AccelerationStructure &as) {
  VkAccelerationStructureGeometryInstancesDataKHR instances{};
  instances.sType =
      VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
  instances.data.deviceAddress = instance_address;

  VkAccelerationStructureGeometryKHR geometry{};
  geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
  geometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
  geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
  geometry.geometry.instances = instances;

  VkAccelerationStructureBuildRangeInfoKHR offset{};
  offset.primitiveCount = instance_count;

  VkAccelerationStructureBuildGeometryInfoKHR build_info{};
  build_info.sType =
      VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
  build_info.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
  build_info.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
  build_info.flags = flags;
  build_info.geometryCount = 1;
  build_info.pGeometries = &geometry;

  VkAccelerationStructureBuildSizesInfoKHR size_info{};
  size_info.sType =
      VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;

  vkGetAccelerationStructureBuildSizesKHR(
      context.device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
      &build_info, &offset.primitiveCount, &size_info);

  CreateBuffer(context, size_info.accelerationStructureSize,
               VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                   VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR,
               VMA_MEMORY_USAGE_GPU_ONLY, as.buffer);

  VkAccelerationStructureCreateInfoKHR as_ci{};
  as_ci.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
  as_ci.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
  as_ci.size = size_info.accelerationStructureSize;
  as_ci.buffer = as.buffer.buffer;

  vkCreateAccelerationStructureKHR(context.device, &as_ci, nullptr, &as.obj);

  AllocatedBuffer scratch_buffer{};
  CreateBuffer(context, size_info.buildScratchSize,
               VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
               VMA_MEMORY_USAGE_GPU_ONLY, scratch_buffer);

  VkDeviceAddress scratch_address =
      GetDeviceAddress(context, scratch_buffer.buffer);

  build_info.dstAccelerationStructure = as.obj;
  build_info.scratchData.deviceAddress = scratch_address;

  std::vector<VkAccelerationStructureBuildRangeInfoKHR *> range_info = {
      &offset,
  };

  ImmediateSubmit::SubmitAsync(context, [&](VkCommandBuffer cmd) {
    vkCmdBuildAccelerationStructuresKHR(cmd, 1, &build_info, range_info.data());
  });

  DestroyBuffer(context, scratch_buffer);
}

void DestroyAccelerationStructure(VulkanContext &context,
                                  AccelerationStructure &as) {
  vkDestroyAccelerationStructureKHR(context.device, as.obj, nullptr);
  DestroyBuffer(context, as.buffer);
}
