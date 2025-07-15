#pragma once

#include "Backend/acceleration_structure.h"
#include "Backend/buffer.h"
#include "Backend/context.h"
#include "Backend/descriptors.h"
#include "Managers/texture_manager.h"
#include "Parsers/asset.h"
#include "Parsers/model.h"
#include "types.h"
#include <filesystem>
#include <glm/vec3.hpp>
#include <queue>
#include <vector>

const uint32_t SCENE_MAX_OBJECTS = 2048;
const uint32_t SCENE_MAX_INSTANCES = 4096;
const uint32_t SCENE_MAX_INDICES = 10'000'000;

struct SceneManager {
  AllocatedBuffer object_buffer;
  AllocatedBuffer mesh_buffer;
  AllocatedBuffer sphere_bounds_buffer;
  AllocatedBuffer instance_buffer;
  AllocatedBuffer aabb_bounds_buffer;
  AllocatedBuffer index_buffer;

  std::vector<Mesh> meshes;
  std::vector<AccelerationStructure> bottom_level_as_vector;
  std::vector<Instance> instances;
  std::vector<MaterialData> materials;
  std::queue<uint32_t> changed_instances;

  AccelerationStructure top_level_as;

  VkDescriptorSet object_descriptor_set;
  VkDescriptorSetLayout object_descriptor_layout;

  VkDescriptorSet instance_descriptor_set;
  VkDescriptorSetLayout instance_descriptor_layout;

  VkDescriptorSet as_descriptor_set;
  VkDescriptorSetLayout as_descriptor_layout;

  std::mutex object_mutex;
  std::mutex instance_mutex;
  std::mutex as_mutex;

  uint32_t object_index;
  uint32_t instance_index;
  uint32_t last_index;

  void Init(VulkanContext &context, DescriptorBuilder &descriptor_builder);

  uint32_t AddObject(VulkanContext &context, MeshData &mesh_data,
                     Material material);
  uint32_t AddObject(VulkanContext &context, AssetData &asset_data,
                     Material material);

  uint32_t AddInstance(VulkanContext &context, Instance &instance);

  void UpdateInstance(glm::mat4 new_matrix, uint32_t index);

  void UpdateInstances(VulkanContext &context);

  void RecreateTopLevelAS(VulkanContext &context);

  void Serialize(VulkanContext &vulkan_context,
                 std::filesystem::path file_path);

  void Deserialize(VulkanContext &vulkan_context,
                   TextureManager &texture_manager,
                   std::filesystem::path file_path);

  void Destroy(VulkanContext &context);
};
