#include "scene_manager.h"
#include "Backend/acceleration_structure.h"
#include "Backend/buffer.h"
#include "Backend/context.h"
#include "Backend/util.h"
#include "Managers/texture_manager.h"
#include "Parsers/asset.h"
#include "Parsers/model.h"
#include "UI/console.h"
#include "fmt/format.h"
#include "types.h"
#include <cassert>
#include <fmt/base.h>
#include <fstream>
#include <mutex>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>

void SceneManager::Init(DescriptorBuilder &descriptor_builder) {
  CreateBuffer(sizeof(Object) * SCENE_MAX_OBJECTS,
               VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
               VMA_MEMORY_USAGE_GPU_ONLY, object_buffer);
  CreateBuffer(sizeof(Mesh) * SCENE_MAX_OBJECTS,
               VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
               VMA_MEMORY_USAGE_GPU_ONLY, mesh_buffer);
  CreateBuffer(sizeof(SphereBounds) * SCENE_MAX_OBJECTS,
               VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
               VMA_MEMORY_USAGE_GPU_ONLY, sphere_bounds_buffer);
  CreateBuffer(sizeof(AabbBounds) * SCENE_MAX_OBJECTS,
               VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
               VMA_MEMORY_USAGE_GPU_ONLY, aabb_bounds_buffer);

  CreateBuffer(sizeof(VkAccelerationStructureInstanceKHR) * SCENE_MAX_INSTANCES,
               VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                   VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
                   VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
               VMA_MEMORY_USAGE_GPU_ONLY, instance_buffer);

  CreateBuffer(sizeof(uint32_t) * SCENE_MAX_INDICES,
               VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                   VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                   VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
                   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
               VMA_MEMORY_USAGE_GPU_ONLY, index_buffer);

  CreateTopLevelAS(0, 0, VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR, top_level_as);

  descriptor_builder.BindStorageBuffer(0, object_buffer.buffer);
  descriptor_builder.BindStorageBuffer(1, mesh_buffer.buffer);
  descriptor_builder.BindStorageBuffer(2, sphere_bounds_buffer.buffer);
  descriptor_builder.BindStorageBuffer(3, index_buffer.buffer);
  descriptor_builder.Build(VK_SHADER_STAGE_ALL, object_descriptor_set, object_descriptor_layout);

  descriptor_builder.BindStorageBuffer(0, instance_buffer.buffer);
  descriptor_builder.Build(VK_SHADER_STAGE_ALL, instance_descriptor_set,
                           instance_descriptor_layout);

  descriptor_builder.BindAccelerationStructure(0, top_level_as.obj);
  descriptor_builder.Build(VK_SHADER_STAGE_ALL, as_descriptor_set, as_descriptor_layout);
}

void SceneManager::AddSceneNode(SceneNodeData &root_node, TextureManager &texture_manager) {
  root_scene_nodes.push_back({});
  SceneNode &new_node = root_scene_nodes.back();
  new_node.children.reserve(root_node.children.size() + root_node.mesh_data.primitives.size());
  new_node.name = fmt::format("Instance ({})", root_scene_nodes.size() - 1);

  for (auto &primitive_data : root_node.mesh_data.primitives) {
    SceneNode new_child_node{};
    new_child_node.instance_index =
        AddObject(primitive_data, texture_manager.UploadMaterial(primitive_data.material_data));
    new_node.children.push_back(new_child_node);
    new_node.name = fmt::format("Primitive ({})", new_node.children.size() - 1);
  }

  for (auto &node : root_node.children) {
    AddChildSceneNode(node, texture_manager, new_node);
  }
}

void SceneManager::AddChildSceneNode(SceneNodeData &root_node, TextureManager &texture_manager,
                                     SceneNode &parent) {
  parent.children.push_back({});
  SceneNode &new_node = parent.children.back();
  new_node.children.reserve(root_node.children.size() + root_node.mesh_data.primitives.size());
  new_node.name = parent.name + fmt::format(" Child ({})", parent.children.size() - 1);

  for (auto &primitive_data : root_node.mesh_data.primitives) {
    SceneNode new_child_node{};
    new_child_node.instance_index =
        AddObject(primitive_data, texture_manager.UploadMaterial(primitive_data.material_data));
    new_child_node.name = fmt::format("Primitive ({})", new_node.children.size());
    new_node.children.push_back(new_child_node);
  }

  for (auto &node : root_node.children) {
    AddChildSceneNode(node, texture_manager, new_node);
  }
}

uint32_t SceneManager::_AddObject(AssetData &asset_data, Material material) {
  uint32_t index;
  uint32_t indice_index;
  {
    std::lock_guard<std::mutex> lock(object_mutex);
    materials.push_back(asset_data.material_data);
    index = object_index;
    object_index++;
    indice_index = last_index;
    last_index += asset_data.indices.size();
  }
  assert(index < SCENE_MAX_OBJECTS && "Reached max object for the scene");

  const size_t index_buffer_size = asset_data.indices.size() * sizeof(uint32_t);
  UpdateBufferAsync(asset_data.indices.data(), index_buffer_size, sizeof(uint32_t) * indice_index,
                    index_buffer);

  Object object{};
  object.material = material;
  UpdateBufferAsync(&object, sizeof(Object), index * sizeof(Object), object_buffer);

  SphereBounds sphere_bounds{};
  AabbBounds aabb_bounds{};
  GetMeshBounds(asset_data.vertices, sphere_bounds, aabb_bounds);

  UpdateBufferAsync(&sphere_bounds, sizeof(SphereBounds), index * sizeof(SphereBounds),
                    sphere_bounds_buffer);

  UpdateBufferAsync(&aabb_bounds, sizeof(AabbBounds), index * sizeof(AabbBounds),
                    aabb_bounds_buffer);

  Mesh mesh{};
  const size_t vertex_buffer_size = asset_data.vertices.size() * sizeof(Vertex);
  CreateBufferDataAsync(asset_data.vertices.data(), vertex_buffer_size,
                        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                            VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
                        mesh.vertex_buffer);
  mesh.index_count = asset_data.indices.size();
  mesh.first_index = indice_index;

  GpuMesh gpu_mesh{};
  gpu_mesh.index_count = asset_data.indices.size();
  gpu_mesh.first_index = indice_index;

  VkBufferDeviceAddressInfo device_address_info{};
  device_address_info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
  device_address_info.buffer = mesh.vertex_buffer.buffer;
  gpu_mesh.vertex_address = vkGetBufferDeviceAddress(VulkanContext::device, &device_address_info);

  UpdateBufferAsync(&gpu_mesh, sizeof(GpuMesh), index * sizeof(GpuMesh), mesh_buffer);

  AccelerationStructure as;
  CreateBottomLevelAS(mesh, GetDeviceAddress(index_buffer.buffer),
                      VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR, as);
  {
    std::lock_guard<std::mutex> lock(as_mutex);
    bottom_level_as_vector.push_back(as);
  }

  std::lock_guard<std::mutex> lock(object_mutex);
  meshes.push_back(mesh);
  return index;
}

uint32_t SceneManager::AddObject(PrimitiveData &mesh_data, Material material) {
  uint32_t index;
  uint32_t indice_index;
  {
    std::lock_guard<std::mutex> lock(object_mutex);
    materials.push_back(mesh_data.material_data);
    index = object_index;
    object_index++;
    indice_index = last_index;
    last_index += mesh_data.indices.size();
  }
  assert(index < SCENE_MAX_OBJECTS && "Reached max object for the scene");

  const size_t index_buffer_size = mesh_data.indices.size() * sizeof(uint32_t);
  UpdateBufferAsync(mesh_data.indices.data(), index_buffer_size, sizeof(uint32_t) * indice_index,
                    index_buffer);

  Object object{};
  object.material = material;
  UpdateBufferAsync(&object, sizeof(Object), index * sizeof(Object), object_buffer);

  UpdateBufferAsync(&mesh_data.sphere_bounds, sizeof(SphereBounds), index * sizeof(SphereBounds),
                    sphere_bounds_buffer);

  UpdateBufferAsync(&mesh_data.aabb_bounds, sizeof(AabbBounds), index * sizeof(AabbBounds),
                    aabb_bounds_buffer);

  Mesh mesh{};
  const size_t vertex_buffer_size = mesh_data.vertices.size() * sizeof(Vertex);
  CreateBufferDataAsync(mesh_data.vertices.data(), vertex_buffer_size,
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
  gpu_mesh.vertex_address = vkGetBufferDeviceAddress(VulkanContext::device, &device_address_info);

  UpdateBufferAsync(&gpu_mesh, sizeof(GpuMesh), index * sizeof(GpuMesh), mesh_buffer);

  AccelerationStructure as;
  CreateBottomLevelAS(mesh, GetDeviceAddress(index_buffer.buffer),
                      VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR, as);
  {
    std::lock_guard<std::mutex> lock(as_mutex);
    bottom_level_as_vector.push_back(as);
  }

  for (auto &instance : mesh_data.instances) {
    Instance new_instance{};
    new_instance.matrix = instance;
    new_instance.object_index = index;
    AddInstance(new_instance);
  }

  std::lock_guard<std::mutex> lock(object_mutex);
  meshes.push_back(mesh);
  return index;
}

uint32_t SceneManager::AddInstance(Instance &instance) {
  uint32_t index;
  {
    std::lock_guard<std::mutex> lock(instance_mutex);
    instances.push_back(instance);
    index = instance_index;
  }

  assert(index <= SCENE_MAX_INSTANCES && "Reached max instances for the scene");

  VkAccelerationStructureInstanceKHR gpu_instance{};
  gpu_instance.transform = Mat4ToVkTransform(instance.matrix);
  gpu_instance.instanceCustomIndex = instance.object_index;
  gpu_instance.mask = 0xFF;
  gpu_instance.instanceShaderBindingTableRecordOffset = 0;
  gpu_instance.flags = 0;
  gpu_instance.accelerationStructureReference =
      GetDeviceAddress(bottom_level_as_vector[instance.object_index].obj);

  UpdateBufferAsync(&gpu_instance, sizeof(VkAccelerationStructureInstanceKHR),
                    index * sizeof(VkAccelerationStructureInstanceKHR), instance_buffer);

  RecreateTopLevelAS();

  {
    std::lock_guard<std::mutex> lock(instance_mutex);
    return instance_index++;
  }
}

void SceneManager::RecreateTopLevelAS() {
  std::lock_guard<std::mutex> lock(as_mutex);
  DestroyAccelerationStructure(top_level_as);
  CreateTopLevelAS(GetDeviceAddress(instance_buffer.buffer), instances.size(),
                   VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR, top_level_as);

  VkWriteDescriptorSetAccelerationStructureKHR as_info{};
  as_info.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
  as_info.accelerationStructureCount = 1;
  as_info.pAccelerationStructures = &top_level_as.obj;

  VkWriteDescriptorSet write{};
  write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  write.descriptorCount = 1;
  write.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
  write.dstBinding = 0;
  write.dstSet = as_descriptor_set;
  write.pNext = &as_info;

  vkUpdateDescriptorSets(VulkanContext::device, 1, &write, 0, nullptr);
}

void SceneManager::UpdateInstance(glm::mat4 new_matrix, uint32_t index) {
  std::lock_guard<std::mutex> lock(instance_mutex);
  instances[index].matrix = new_matrix;
  changed_instances.push_back(index);
}

void SceneManager::UpdateInstances() {
  if (changed_instances.empty())
    return;
  std::thread([&]() {
    std::lock_guard<std::mutex> lock(instance_mutex);
    for (uint32_t i = 0; i < changed_instances.size(); i++) {
      uint32_t instance_index = changed_instances[i];

      VkTransformMatrixKHR transform_matrix = Mat4ToVkTransform(instances[instance_index].matrix);

      UpdateBufferAsync(&transform_matrix, sizeof(VkTransformMatrixKHR),
                        instance_index * sizeof(VkAccelerationStructureInstanceKHR),
                        instance_buffer);
    }

    changed_instances.clear();

    RecreateTopLevelAS();
  }).detach();
}

struct SceneLut {
  size_t object_files_offset;
  size_t instance_offset;
};

void SceneManager::Serialize(std::filesystem::path file_path) {
  std::ofstream file(file_path.string(), std::ios::ate | std::ios::out | std::ios::binary);

  if (!file) {
    fmt::println("[ERROR] failed to open file {}... trying again", file_path.string());
    file.open(file_path.string(), std::ios::ate | std::ios::out | std::ios::binary);
    if (!file) {
      fmt::println("[ERROR] failed to open file again {}... returning", file_path.string());
      return;
    }
  }

  std::vector<std::string> object_files;
  object_files.resize(object_index);

  for (uint32_t i = 0; i < object_index; i++) {
    object_files[i] = fmt::format("Meshes/{}.mesh", i);
    SerializeAsset(index_buffer, meshes[i], materials[i], object_files[i]);
  }

  SceneLut scene_lut;
  scene_lut.object_files_offset = sizeof(SceneLut);
  size_t object_files_size = 0;
  for (auto &object_file : object_files) {
    object_files_size += object_file.size() + 1;
  }
  scene_lut.instance_offset = scene_lut.object_files_offset + object_files_size;

  file.write((const char *)&scene_lut, sizeof(SceneLut));

  for (auto &object_file : object_files) {
    file.write(object_file.data(), object_file.size() + 1);
  }

  const size_t instance_count = instances.size();
  file.write((const char *)&instance_count, sizeof(size_t));

  file.write((const char *)instances.data(), instance_count * sizeof(Instance));

  file.close();
}

void SceneManager::Deserialize(TextureManager &texture_manager, std::filesystem::path file_path) {
  std::ifstream file(file_path.string(), std::ios::ate | std::ios::binary);
  if (!file.is_open()) {
    fmt::println("[ERROR], failed to desrialize scene");
    return;
  }
  size_t file_size = (size_t)file.tellg();

  std::vector<char> buffer(file_size);

  file.seekg(0);
  file.read((char *)buffer.data(), file_size);
  file.close();

  std::vector<std::string> asset_files;
  asset_files.push_back("");
  SceneLut scene_lut = *(SceneLut *)buffer.data();

  for (size_t i = scene_lut.object_files_offset; i < scene_lut.instance_offset; i++) {
    asset_files.back() += buffer[i];
    if (buffer[i] == '\0') {
      asset_files.push_back("");
    }
  }
  asset_files.pop_back();

  std::vector<AssetData> asset_datas;
  asset_datas.reserve(asset_files.size());
  for (auto &asset_file : asset_files) {
    asset_datas.push_back(ParseAsset(asset_file));
  }

  for (auto &asset_data : asset_datas) {
    Material material = texture_manager.UploadMaterial(asset_data.material_data);
    _AddObject(asset_data, material);
  }

  size_t instance_count = *(size_t *)&buffer[scene_lut.instance_offset];
  fmt::println("{}", instance_count);

  for (size_t i = 0; i < instance_count; i++) {
    Instance instance =
        *(Instance *)&buffer[i * sizeof(Instance) + scene_lut.instance_offset + sizeof(size_t)];
    AddInstance(instance);
  }
}

void SceneManager::Destroy() {
  std::lock_guard<std::mutex> as_lock(as_mutex);
  std::lock_guard<std::mutex> instance_lock(instance_mutex);
  std::lock_guard<std::mutex> object_lock(object_mutex);

  for (auto &mesh : meshes) {
    DestroyBuffer(mesh.vertex_buffer);
  }

  for (auto &bottom_level_as : bottom_level_as_vector) {
    DestroyAccelerationStructure(bottom_level_as);
  }

  DestroyAccelerationStructure(top_level_as);

  DestroyBuffer(sphere_bounds_buffer);
  DestroyBuffer(aabb_bounds_buffer);
  DestroyBuffer(mesh_buffer);
  DestroyBuffer(object_buffer);
  DestroyBuffer(instance_buffer);
  DestroyBuffer(index_buffer);

  vkDestroyDescriptorSetLayout(VulkanContext::device, object_descriptor_layout, nullptr);
  vkDestroyDescriptorSetLayout(VulkanContext::device, as_descriptor_layout, nullptr);
  vkDestroyDescriptorSetLayout(VulkanContext::device, instance_descriptor_layout, nullptr);
}
