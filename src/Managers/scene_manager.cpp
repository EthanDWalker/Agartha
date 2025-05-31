#include "scene_manager.h"
#include "Backend/buffer.h"
#include <cassert>

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
  CreateBuffer(context, sizeof(AABB) * SCENE_MAX_OBJECTS,
               VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                   VK_BUFFER_USAGE_TRANSFER_DST_BIT,
               VMA_MEMORY_USAGE_GPU_ONLY, aabb_buffer);

  CreateBuffer(context, sizeof(Instance) * SCENE_MAX_INSTANCES,
               VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                   VK_BUFFER_USAGE_TRANSFER_DST_BIT,
               VMA_MEMORY_USAGE_GPU_ONLY, instance_buffer);

  descriptor_builder.Reset();
  descriptor_builder.BindStorageBuffer(0, object_buffer.buffer);
  descriptor_builder.BindStorageBuffer(1, mesh_buffer.buffer);
  descriptor_builder.BindStorageBuffer(2, aabb_buffer.buffer);
  descriptor_builder.Build(context, VK_SHADER_STAGE_ALL, object_descriptor_set,
                           object_descriptor_layout);

  descriptor_builder.Reset();
  descriptor_builder.BindStorageBuffer(0, instance_buffer.buffer);
  descriptor_builder.Build(context, VK_SHADER_STAGE_ALL,
                           instance_descriptor_set, instance_descriptor_layout);
}

uint32_t SceneManager::AddObject(VulkanContext &context,
                                 ImmediateSubmit &immediate_submit,
                                 MeshData &mesh_data, Material &material) {
  uint32_t index;
  if (removed_objects.empty()) {
    index = object_index;
  } else {
    index = removed_objects.front();
  }

  assert(index < SCENE_MAX_OBJECTS && "Reached max object for the scene");

  Object object{};
  object.material = material;

  UpdateBuffer(context, immediate_submit, &object, sizeof(Object),
               index * sizeof(Object), object_buffer);

  AABB aabb{};
  aabb.min = mesh_data.collilder_min;
  aabb.max = mesh_data.collilder_max;

  UpdateBuffer(context, immediate_submit, &aabb, sizeof(AABB),
               index * sizeof(AABB), aabb_buffer);

  Mesh mesh{};

  const size_t vertex_buffer_size = mesh_data.vertices.size() * sizeof(Vertex);
  const size_t index_buffer_size = mesh_data.indices.size() * sizeof(uint32_t);

  CreateBufferData(context, immediate_submit, mesh_data.vertices.data(),
                   vertex_buffer_size,
                   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                       VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                   mesh.vertex_buffer);

  CreateBufferData(context, immediate_submit, mesh_data.indices.data(),
                   index_buffer_size,
                   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                       VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                   mesh.index_buffer);

  meshes.push_back(mesh);

  GpuMesh gpu_mesh{};

  VkBufferDeviceAddressInfo device_address_info{};
  device_address_info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
  device_address_info.buffer = mesh.vertex_buffer.buffer;

  gpu_mesh.vertex_address =
      vkGetBufferDeviceAddress(context.device, &device_address_info);

  device_address_info.buffer = mesh.index_buffer.buffer;

  gpu_mesh.index_address =
      vkGetBufferDeviceAddress(context.device, &device_address_info);

  gpu_mesh.index_count = mesh_data.indices.size();

  UpdateBuffer(context, immediate_submit, &gpu_mesh, sizeof(GpuMesh),
               index * sizeof(GpuMesh), mesh_buffer);

  for (auto &instance : mesh_data.instances) {
    Instance new_instance{};
    new_instance.matrix = instance;
    new_instance.object_index = index;
    new_instance.color = glm::vec3(1.0);
    AddInstance(context, immediate_submit, &new_instance);
  }

  if (removed_objects.empty()) {
    return object_index++;
  } else {
    removed_objects.pop();
    return index;
  }
}

void SceneManager::RemoveObject(VulkanContext &context,
                                ImmediateSubmit &immediate_submit,
                                uint32_t index) {
  assert(index < object_index && "Cannot remove unused index");

  AABB zero_aabb{};
  UpdateBuffer(context, immediate_submit, &zero_aabb, sizeof(AABB),
               index * sizeof(AABB), aabb_buffer);

  Mesh zero_mesh{};
  UpdateBuffer(context, immediate_submit, &zero_mesh, sizeof(Mesh),
               index * sizeof(Mesh), mesh_buffer);

  Object zero_object{};
  UpdateBuffer(context, immediate_submit, &zero_object, sizeof(Object),
               index * sizeof(Object), object_buffer);

  removed_objects.push(index);
}

uint32_t SceneManager::AddInstance(VulkanContext &context,
                                   ImmediateSubmit &immediate_submit,
                                   Instance *instance) {
  uint32_t index;
  if (removed_instances.empty()) {
    index = instance_index;
  } else {
    index = removed_instances.front();
  }

  assert(index <= SCENE_MAX_INSTANCES && "Reached max instances for the scene");

  UpdateBuffer(context, immediate_submit, instance, sizeof(Instance),
               index * sizeof(Instance), instance_buffer);

  if (removed_instances.empty()) {
    return instance_index++;
  } else {
    removed_instances.pop();
    return index;
  }
}

void SceneManager::EditInstance(VulkanContext &context,
                                ImmediateSubmit &immediate_submit,
                                Instance *instance, uint32_t index) {
  assert(index < instance_index &&
         "Use SceneManager::AddObject to allow for desired behavior");

  UpdateBuffer(context, immediate_submit, instance, sizeof(Instance),
               index * sizeof(Instance), instance_buffer);
}

void SceneManager::RemoveInstance(VulkanContext &context,
                                  ImmediateSubmit &immediate_submit,
                                  uint32_t index) {
  assert(index < instance_index && "Cannot remove useset index");

  Instance zero_instance{};
  UpdateBuffer(context, immediate_submit, &zero_instance, sizeof(Instance),
               index * sizeof(Instance), instance_buffer);

  removed_instances.push(index);
}

void SceneManager::Destroy(VulkanContext &context) {
  for (auto &mesh : meshes) {
    DestroyBuffer(context, mesh.vertex_buffer);
    DestroyBuffer(context, mesh.index_buffer);
  }

  DestroyBuffer(context, aabb_buffer);
  DestroyBuffer(context, mesh_buffer);
  DestroyBuffer(context, object_buffer);
  DestroyBuffer(context, instance_buffer);

  vkDestroyDescriptorSetLayout(context.device, object_descriptor_layout,
                               nullptr);
  vkDestroyDescriptorSetLayout(context.device, instance_descriptor_layout,
                               nullptr);
}
