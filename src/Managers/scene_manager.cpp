#include "scene_manager.h"
#include "Backend/buffer.h"
#include "Backend/context.h"
#include "Backend/immediate_submit.h"
#include <cassert>
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

  CreateBuffer(context, sizeof(Instance) * SCENE_MAX_INSTANCES,
               VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                   VK_BUFFER_USAGE_TRANSFER_DST_BIT,
               VMA_MEMORY_USAGE_GPU_ONLY, instance_buffer);

  CreateBuffer(
      context, sizeof(uint32_t) * SCENE_MAX_INDICES,
      VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
          VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
          VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
      VMA_MEMORY_USAGE_GPU_ONLY, index_buffer);

  descriptor_builder.Reset();
  descriptor_builder.BindStorageBuffer(0, object_buffer.buffer);
  descriptor_builder.BindStorageBuffer(1, mesh_buffer.buffer);
  descriptor_builder.BindStorageBuffer(2, sphere_bounds_buffer.buffer);
  descriptor_builder.Build(context, VK_SHADER_STAGE_ALL, object_descriptor_set,
                           object_descriptor_layout);

  descriptor_builder.Reset();
  descriptor_builder.BindStorageBuffer(0, instance_buffer.buffer);
  descriptor_builder.Build(context, VK_SHADER_STAGE_ALL,
                           instance_descriptor_set, instance_descriptor_layout);
}

std::vector<uint32_t>
SceneManager::AddObjects(VulkanContext &context, std::vector<MeshData> data,
                         std::vector<Material> materials) {
  uint32_t index;
  index = object_index;

  assert(index + data.size() < SCENE_MAX_OBJECTS &&
         "Reached max object for the scene");
  assert(data.size() == materials.size() &&
         "all objects must have materials if adding in bulk");

  std::vector<Object> objects{};
  objects.resize(data.size());
  std::vector<SphereBounds> sphere_bounds{};
  sphere_bounds.resize(data.size());
  std::vector<Mesh> new_meshes{};
  new_meshes.resize(data.size());
  std::vector<GpuMesh> gpu_meshes{};
  gpu_meshes.resize(data.size());
  std::vector<uint32_t> object_indices{};
  object_indices.resize(data.size());

  for (uint32_t i = 0; i < data.size(); i++) {
    objects[i].material = materials[i];
    sphere_bounds[i].radius = data[i].bounds_radius;

    new_meshes[i].first_index = last_index;
    new_meshes[i].index_count = data[i].indices.size();
    const size_t vertex_buffer_size = data[i].vertices.size() * sizeof(Vertex);
    CreateBufferDataAsync(context, data[i].vertices.data(), vertex_buffer_size,
                          VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                              VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                          new_meshes[i].vertex_buffer);

    VkBufferDeviceAddressInfo device_address_info{};
    device_address_info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    device_address_info.buffer = new_meshes[i].vertex_buffer.buffer;
    gpu_meshes[i].vertex_address =
        vkGetBufferDeviceAddress(context.device, &device_address_info);
    gpu_meshes[i].index_count = data[i].indices.size();
    gpu_meshes[i].first_index = last_index;

    const size_t index_buffer_size = data[i].indices.size() * sizeof(uint32_t);
    UpdateBufferAsync(context, data[i].indices.data(), index_buffer_size,
                      sizeof(uint32_t) * last_index, index_buffer);

    for (auto &instance : data[i].instances) {
      Instance new_instance{};
      new_instance.matrix = instance;
      new_instance.object_index = object_index;
      new_instance.color = glm::vec3(1.0);
      AddInstance(context, &new_instance);
    }

    last_index += data[i].indices.size();
    object_indices[i] = object_index++;
  }

  UpdateBufferAsync(context, objects.data(), sizeof(Object) * objects.size(),
                    index * sizeof(Object), object_buffer);

  UpdateBufferAsync(context, sphere_bounds.data(),
                    sizeof(SphereBounds) * sphere_bounds.size(),
                    index * sizeof(SphereBounds), sphere_bounds_buffer);

  UpdateBufferAsync(context, gpu_meshes.data(),
                    sizeof(GpuMesh) * gpu_meshes.size(),
                    index * sizeof(GpuMesh), mesh_buffer);

  {
    std::lock_guard<std::mutex> lock(object_mutex);
    for (auto &mesh : new_meshes) {
      meshes.push_back(mesh);
    }
  }

  return object_indices;
}

uint32_t SceneManager::AddObject(VulkanContext &context, MeshData &mesh_data,
                                 Material material) {
  uint32_t index;
  {
    std::lock_guard<std::mutex> lock(object_mutex);
    if (removed_objects.empty()) {
      index = object_index;
    } else {
      index = removed_objects.front();
    }
  }
  assert(index < SCENE_MAX_OBJECTS && "Reached max object for the scene");

  const size_t index_buffer_size = mesh_data.indices.size() * sizeof(uint32_t);
  UpdateBufferAsync(context, mesh_data.indices.data(), index_buffer_size,
                    sizeof(uint32_t) * last_index, index_buffer);

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
  mesh.first_index = last_index;
  {
    std::lock_guard<std::mutex> lock(object_mutex);
    meshes.push_back(mesh);
  }

  GpuMesh gpu_mesh{};

  gpu_mesh.index_count = mesh_data.indices.size();
  gpu_mesh.first_index = last_index;

  VkBufferDeviceAddressInfo device_address_info{};
  device_address_info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
  device_address_info.buffer = mesh.vertex_buffer.buffer;
  gpu_mesh.vertex_address =
      vkGetBufferDeviceAddress(context.device, &device_address_info);

  UpdateBufferAsync(context, &gpu_mesh, sizeof(GpuMesh),
                    index * sizeof(GpuMesh), mesh_buffer);

  for (auto &instance : mesh_data.instances) {
    Instance new_instance{};
    new_instance.matrix = instance;
    new_instance.object_index = index;
    new_instance.color = glm::vec3(1.0);
    AddInstance(context, &new_instance);
  }

  last_index += mesh_data.indices.size();

  std::lock_guard<std::mutex> lock(object_mutex);
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

  SphereBounds zero_sphere_bounds{};
  UpdateBuffer(context, immediate_submit, &zero_sphere_bounds,
               sizeof(SphereBounds), index * sizeof(SphereBounds),
               sphere_bounds_buffer);

  Mesh zero_mesh{};
  UpdateBuffer(context, immediate_submit, &zero_mesh, sizeof(Mesh),
               index * sizeof(Mesh), mesh_buffer);

  Object zero_object{};
  UpdateBuffer(context, immediate_submit, &zero_object, sizeof(Object),
               index * sizeof(Object), object_buffer);

  removed_objects.push(index);
}

uint32_t SceneManager::AddInstance(VulkanContext &context, Instance *instance) {
  uint32_t index;
  {
    std::lock_guard<std::mutex> lock(instance_mutex);
    if (removed_instances.empty()) {
      index = instance_index;
    } else {
      index = removed_instances.front();
    }
  }
  assert(index <= SCENE_MAX_INSTANCES && "Reached max instances for the scene");

  UpdateBufferAsync(context, instance, sizeof(Instance),
                    index * sizeof(Instance), instance_buffer);

  {
    std::lock_guard<std::mutex> lock(instance_mutex);
    if (removed_instances.empty()) {
      return instance_index++;
    } else {
      removed_instances.pop();
      return index;
    }
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
  assert(index < instance_index && "Cannot remove unset index");

  Instance zero_instance{};
  UpdateBuffer(context, immediate_submit, &zero_instance, sizeof(Instance),
               index * sizeof(Instance), instance_buffer);

  removed_instances.push(index);
}

void SceneManager::Destroy(VulkanContext &context) {
  for (auto &mesh : meshes) {
    DestroyBuffer(context, mesh.vertex_buffer);
  }

  DestroyBuffer(context, sphere_bounds_buffer);
  DestroyBuffer(context, aabb_bounds_buffer);
  DestroyBuffer(context, mesh_buffer);
  DestroyBuffer(context, object_buffer);
  DestroyBuffer(context, instance_buffer);
  DestroyBuffer(context, index_buffer);

  vkDestroyDescriptorSetLayout(context.device, object_descriptor_layout,
                               nullptr);
  vkDestroyDescriptorSetLayout(context.device, instance_descriptor_layout,
                               nullptr);
}
