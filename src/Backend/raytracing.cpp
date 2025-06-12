#include "raytracing.h"
#include "buffer.h"
#include "context.h"
#include "immediate_submit.h"
#include "util.h"

void ASBuilder::SetMesh(VulkanContext &context, Mesh &mesh,
                        VkDeviceAddress index_address) {
  VkDeviceAddress vertex_address =
      GetDeviceAddress(context, mesh.vertex_buffer);

  VkAccelerationStructureGeometryTrianglesDataKHR triangles{};
  triangles.sType =
      VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
  triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
  triangles.vertexData.deviceAddress = vertex_address;
  triangles.vertexStride = sizeof(Vertex);
  triangles.indexType = VK_INDEX_TYPE_UINT32;
  triangles.indexData.deviceAddress = index_address;
  triangles.maxVertex = (mesh.vertex_buffer.info.size / sizeof(Vertex)) - 1;

  geomertry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
  geomertry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
  geomertry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
  geomertry.geometry.triangles = triangles;

  offset.primitiveCount = mesh.index_count / 3;
  offset.primitiveOffset = mesh.first_index / 3;
}

void ASBuilder::SetInstances(VulkanContext &context,
                             AllocatedBuffer instance_buffer) {
  VkAccelerationStructureGeometryInstancesDataKHR instances{};
  instances.sType =
      VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
  instances.data.deviceAddress = GetDeviceAddress(context, instance_buffer);

  geomertry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
  geomertry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
  geomertry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;

  offset.primitiveCount = instance_buffer.info.size / sizeof(Instance);
}

AccelerationStructure
ASBuilder::CreateBottomLevelAS(VulkanContext &context,
                               VkBuildAccelerationStructureFlagsKHR flags) {
  AccelerationStructure as;

  VkAccelerationStructureBuildGeometryInfoKHR build_info{};
  build_info.sType =
      VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
  build_info.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
  build_info.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
  build_info.flags = flags;
  build_info.geometryCount = 1;
  build_info.pGeometries = &geomertry;

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

  VkDeviceAddress as_buffer_address = GetDeviceAddress(context, as.buffer);

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

  VkDeviceAddress scratch_address = GetDeviceAddress(context, scratch_buffer);

  build_info.dstAccelerationStructure = as.obj;
  build_info.scratchData.deviceAddress = scratch_address;

  std::vector<VkAccelerationStructureBuildRangeInfoKHR *> range_info = {
      &offset,
  };

  ImmediateSubmit::SubmitAsync(context, [&](VkCommandBuffer cmd) {
    vkCmdBuildAccelerationStructuresKHR(cmd, 1, &build_info, range_info.data());
  });

  DestroyBuffer(context, scratch_buffer);

  return as;
}

AccelerationStructure
ASBuilder::CreateTopLevelAS(VulkanContext &context,
                            VkBuildAccelerationStructureFlagsKHR flags) {
  AccelerationStructure as;

  VkAccelerationStructureBuildGeometryInfoKHR build_info{};
  build_info.sType =
      VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
  build_info.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
  build_info.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
  build_info.flags = flags;
  build_info.geometryCount = 1;
  build_info.pGeometries = &geomertry;

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

  VkDeviceAddress as_buffer_address = GetDeviceAddress(context, as.buffer);

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

  VkDeviceAddress scratch_address = GetDeviceAddress(context, scratch_buffer);

  build_info.dstAccelerationStructure = as.obj;
  build_info.scratchData.deviceAddress = scratch_address;

  std::vector<VkAccelerationStructureBuildRangeInfoKHR *> range_info = {
      &offset,
  };

  ImmediateSubmit::SubmitAsync(context, [&](VkCommandBuffer cmd) {
    vkCmdBuildAccelerationStructuresKHR(cmd, 1, &build_info, range_info.data());
  });

  DestroyBuffer(context, scratch_buffer);

  return as;
}

void DestroyAccelerationStructure(VulkanContext &context,
                                  AccelerationStructure &as) {
  vkDestroyAccelerationStructureKHR(context.device, as.obj, nullptr);
  DestroyBuffer(context, as.buffer);
}
