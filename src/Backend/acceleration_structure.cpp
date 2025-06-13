#include "acceleration_structure.h"
#include "buffer.h"
#include "context.h"
#include "immediate_submit.h"
#include "util.h"
#include <span>
#include <vector>

void ASBuilder::SetMesh(VulkanContext &context, Mesh &mesh,
                        VkDeviceAddress index_address) {
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

  geomertry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
  geomertry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
  geomertry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
  geomertry.geometry.triangles = triangles;

  offset.primitiveCount = mesh.index_count / 3;
  offset.primitiveOffset = mesh.first_index / 3;
}

VkTransformMatrixKHR Mat4ToVkTransform(glm::mat4 matrix) {
  VkTransformMatrixKHR transform;
  for (uint8_t i = 0; i < 3; i++) {
    for (uint8_t j = 0; j < 4; j++) {
      transform.matrix[i][j] = matrix[i][j];
    }
  }

  return transform;
}

void ASBuilder::SetInstances(VulkanContext &context,
                             std::span<Instance> instance_data,
                             AccelerationStructure bottom_level_as) {
  VkDeviceAddress bottom_level_as_address =
      GetDeviceAddress(context, bottom_level_as.obj);

  std::vector<VkAccelerationStructureInstanceKHR> as_instances{};
  as_instances.reserve(instance_data.size());
  for (auto &instance : instance_data) {
    VkAccelerationStructureInstanceKHR as_instance{};
    as_instance.transform = Mat4ToVkTransform(instance.matrix);
    as_instance.instanceCustomIndex = instance.object_index;
    as_instance.mask = 0xFF;
    as_instance.instanceShaderBindingTableRecordOffset = 0;
    as_instance.flags =
        VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
    as_instance.accelerationStructureReference = bottom_level_as_address;
    as_instances.push_back(as_instance);
  }

  if (instance_buffer.buffer != VK_NULL_HANDLE) {
    DestroyBuffer(context, instance_buffer);
  }

  CreateBufferDataAsync(
      context, as_instances.data(),
      sizeof(VkAccelerationStructureInstanceKHR) * as_instances.size(),
      VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
          VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
      instance_buffer);

  VkAccelerationStructureGeometryInstancesDataKHR instances{};
  instances.sType =
      VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
  instances.data.deviceAddress =
      GetDeviceAddress(context, instance_buffer.buffer);

  geomertry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
  geomertry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
  geomertry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
  geomertry.geometry.instances = instances;

  offset.primitiveCount = instance_data.size();
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
  DestroyBuffer(context, instance_buffer);

  return as;
}

void DestroyAccelerationStructure(VulkanContext &context,
                                  AccelerationStructure &as) {
  vkDestroyAccelerationStructureKHR(context.device, as.obj, nullptr);
  DestroyBuffer(context, as.buffer);
}
