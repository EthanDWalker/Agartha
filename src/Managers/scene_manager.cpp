#include "scene_manager.h"
#include "Backend/acceleration_structure.h"
#include "Backend/buffer.h"
#include "Backend/context.h"
#include "Backend/util.h"
#include <cassert>
#include <fmt/base.h>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>
#include <mutex>

void SceneManager::Init(VulkanContext &context,
                        DescriptorBuilder &descriptor_builder) {
  CreateBuffer(context, sizeof(Object) * SCENE_MAX_OBJECTS,
               VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                   VK_BUFFER_USAGE_TRANSFER_DST_BIT,
               VMA_MEMORY_USAGE_GPU_ONLY, object_buffer);
  CreateBuffer(context, sizeof(Mesh) * SCENE_MAX_OBJECTS,
               VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                   VK_BUFFER_USAGE_TRANSFER_DST_BIT,
               VMA_MEMORY_USAGE_GPU_ONLY, mesh_buffer);
  CreateBuffer(context, sizeof(SphereBounds) * SCENE_MAX_OBJECTS,
               VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                   VK_BUFFER_USAGE_TRANSFER_DST_BIT,
               VMA_MEMORY_USAGE_GPU_ONLY, sphere_bounds_buffer);
  CreateBuffer(context, sizeof(AabbBounds) * SCENE_MAX_OBJECTS,
               VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                   VK_BUFFER_USAGE_TRANSFER_DST_BIT,
               VMA_MEMORY_USAGE_GPU_ONLY, aabb_bounds_buffer);

  CreateBuffer(
      context, sizeof(VkAccelerationStructureInstanceKHR) * SCENE_MAX_INSTANCES,
      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
          VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
          VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
      VMA_MEMORY_USAGE_GPU_ONLY, instance_buffer);

  CreateBuffer(
      context, sizeof(uint32_t) * SCENE_MAX_INDICES,
      VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
          VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
          VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
          VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
      VMA_MEMORY_USAGE_GPU_ONLY, index_buffer);

  CreateTopLevelAS(context, 0, 0,
                   VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR,
                   top_level_as);

  descriptor_builder.BindStorageBuffer(0, object_buffer.buffer);
  descriptor_builder.BindStorageBuffer(1, mesh_buffer.buffer);
  descriptor_builder.BindStorageBuffer(2, sphere_bounds_buffer.buffer);
  descriptor_builder.BindStorageBuffer(3, index_buffer.buffer);
  descriptor_builder.Build(context, VK_SHADER_STAGE_ALL, object_descriptor_set,
                           object_descriptor_layout);

  descriptor_builder.BindStorageBuffer(0, instance_buffer.buffer);
  descriptor_builder.Build(context, VK_SHADER_STAGE_ALL,
                           instance_descriptor_set, instance_descriptor_layout);

  descriptor_builder.BindAccelerationStructure(0, top_level_as.obj);
  descriptor_builder.Build(context, VK_SHADER_STAGE_ALL, as_descriptor_set,
                           as_descriptor_layout);
}

uint32_t SceneManager::AddObject(VulkanContext &context, MeshData &mesh_data,
                                 Material material) {
  uint32_t index;
  uint32_t indice_index;
  {
    std::lock_guard<std::mutex> lock(object_mutex);
    index = object_index;
    object_index++;
    indice_index = last_index;
    last_index += mesh_data.indices.size();
  }
  assert(index < SCENE_MAX_OBJECTS && "Reached max object for the scene");

  const size_t index_buffer_size = mesh_data.indices.size() * sizeof(uint32_t);
  UpdateBufferAsync(context, mesh_data.indices.data(), index_buffer_size,
                    sizeof(uint32_t) * indice_index, index_buffer);

  Object object{};
  object.material = material;
  UpdateBufferAsync(context, &object, sizeof(Object), index * sizeof(Object),
                    object_buffer);

  SphereBounds sphere_bounds{};
  sphere_bounds.radius = mesh_data.bounds_radius;
  UpdateBufferAsync(context, &sphere_bounds, sizeof(SphereBounds),
                    index * sizeof(SphereBounds), sphere_bounds_buffer);

  AabbBounds aabb_bounds{};
  aabb_bounds.min = glm::vec4(mesh_data.aabb_bounds.first, 0.0);
  aabb_bounds.max = glm::vec4(mesh_data.aabb_bounds.second, 0.0);
  UpdateBufferAsync(context, &aabb_bounds, sizeof(AabbBounds),
                    index * sizeof(AabbBounds), aabb_bounds_buffer);

  Mesh mesh{};
  const size_t vertex_buffer_size = mesh_data.vertices.size() * sizeof(Vertex);
  CreateBufferDataAsync(
      context, mesh_data.vertices.data(), vertex_buffer_size,
      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
          VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
          VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
      mesh.vertex_buffer);
  mesh.index_count = mesh_data.indices.size();
  mesh.first_index = indice_index;

  GpuMesh gpu_mesh{};
  gpu_mesh.index_count = mesh_data.indices.size();
  gpu_mesh.first_index = indice_index;

  VkBufferDeviceAddressInfo device_address_info{};
  device_address_info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
  device_address_info.buffer = mesh.vertex_buffer.buffer;
  gpu_mesh.vertex_address =
      vkGetBufferDeviceAddress(context.device, &device_address_info);

  UpdateBufferAsync(context, &gpu_mesh, sizeof(GpuMesh),
                    index * sizeof(GpuMesh), mesh_buffer);

  AccelerationStructure as;
  CreateBottomLevelAS(
      context, mesh, GetDeviceAddress(context, index_buffer.buffer),
      VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR, as);
  {
    std::lock_guard<std::mutex> lock(as_mutex);
    bottom_level_as_vector.push_back(as);
  }

  for (auto &instance : mesh_data.instances) {
    Instance new_instance{};
    new_instance.matrix = instance;
    new_instance.object_index = index;
    AddInstance(context, new_instance);
  }

  std::lock_guard<std::mutex> lock(object_mutex);
  meshes.push_back(mesh);
  return index;
}

uint32_t SceneManager::AddInstance(VulkanContext &context, Instance &instance) {
  uint32_t index;
  {
    std::lock_guard<std::mutex> lock(instance_mutex);
    instance_matrices.push_back(instance.matrix);
    index = instance_index;
  }

  assert(index <= SCENE_MAX_INSTANCES && "Reached max instances for the scene");

  VkAccelerationStructureInstanceKHR gpu_instance{};
  gpu_instance.transform = Mat4ToVkTransform(instance.matrix);
  gpu_instance.instanceCustomIndex = instance.object_index;
  gpu_instance.mask = 0xFF;
  gpu_instance.instanceShaderBindingTableRecordOffset = 0;
  gpu_instance.flags = 0;
  gpu_instance.accelerationStructureReference = GetDeviceAddress(
      context, bottom_level_as_vector[instance.object_index].obj);

  UpdateBufferAsync(
      context, &gpu_instance, sizeof(VkAccelerationStructureInstanceKHR),
      index * sizeof(VkAccelerationStructureInstanceKHR), instance_buffer);

  {
    std::lock_guard<std::mutex> lock(instance_mutex);
    return instance_index++;
  }
}

void SceneManager::RecreateTopLevelAS(VulkanContext &context) {
  std::lock_guard<std::mutex> lock(as_mutex);
  DestroyAccelerationStructure(context, top_level_as);
  CreateTopLevelAS(context, GetDeviceAddress(context, instance_buffer.buffer),
                   instance_matrices.size(),
                   VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR,
                   top_level_as);

  VkWriteDescriptorSetAccelerationStructureKHR as_info{};
  as_info.sType =
      VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
  as_info.accelerationStructureCount = 1;
  as_info.pAccelerationStructures = &top_level_as.obj;

  VkWriteDescriptorSet write{};
  write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  write.descriptorCount = 1;
  write.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
  write.dstBinding = 0;
  write.dstSet = as_descriptor_set;
  write.pNext = &as_info;

  vkUpdateDescriptorSets(context.device, 1, &write, 0, nullptr);
}

void SceneManager::UpdateInstance(glm::mat4 new_matrix, uint32_t index) {
  instance_matrices[index] = new_matrix;
  changed_instances.push(index);
}

void SceneManager::UpdateInstances(VulkanContext &context) {
  if (changed_instances.size() == 0)
    return;
  std::thread([&]() {
    for (uint32_t i = 0; i < changed_instances.size(); i++) {
      uint32_t instance_index = changed_instances.front();

      VkTransformMatrixKHR transform_matrix =
          Mat4ToVkTransform(instance_matrices[instance_index]);

      UpdateBufferAsync(
          context, &transform_matrix, sizeof(VkTransformMatrixKHR),
          instance_index * sizeof(VkAccelerationStructureInstanceKHR),
          instance_buffer);

      changed_instances.pop();
    }

    RecreateTopLevelAS(context);
  }).detach();
}

void SceneManager::Destroy(VulkanContext &context) {
  std::lock_guard<std::mutex> as_lock(as_mutex);
  std::lock_guard<std::mutex> instance_lock(instance_mutex);
  std::lock_guard<std::mutex> object_lock(object_mutex);
  for (auto &mesh : meshes) {
    DestroyBuffer(context, mesh.vertex_buffer);
  }

  for (auto &bottom_level_as : bottom_level_as_vector) {
    DestroyAccelerationStructure(context, bottom_level_as);
  }

  DestroyAccelerationStructure(context, top_level_as);

  DestroyBuffer(context, sphere_bounds_buffer);
  DestroyBuffer(context, aabb_bounds_buffer);
  DestroyBuffer(context, mesh_buffer);
  DestroyBuffer(context, object_buffer);
  DestroyBuffer(context, instance_buffer);
  DestroyBuffer(context, index_buffer);

  vkDestroyDescriptorSetLayout(context.device, object_descriptor_layout,
                               nullptr);
  vkDestroyDescriptorSetLayout(context.device, as_descriptor_layout, nullptr);
  vkDestroyDescriptorSetLayout(context.device, instance_descriptor_layout,
                               nullptr);
}
