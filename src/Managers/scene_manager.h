#pragma once

#include "Backend/acceleration_structure.h"
#include "Backend/buffer.h"
#include "Backend/descriptors.h"
#include "Managers/texture_manager.h"
#include "Parsers/asset.h"
#include "Parsers/model.h"
#include "types.h"
#include <filesystem>
#include <glm/vec3.hpp>
#include <limits>
#include <queue>
#include <vector>

const uint32_t SCENE_MAX_OBJECTS = 2048;
const uint32_t SCENE_MAX_INSTANCES = 4096;
const uint32_t SCENE_MAX_INDICES = 10'000'000;

struct SceneNode {
  std::string name;
  std::vector<SceneNode> children;
  uint32_t instance_index{std::numeric_limits<uint32_t>::max()};
};

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
  std::vector<SceneNode> root_scene_nodes;
  std::vector<MaterialData> materials;
  std::vector<uint32_t> changed_instances;

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

  void Init(DescriptorBuilder &descriptor_builder);

  void AddSceneNode(SceneNodeData &root_node, TextureManager &texture_manager);

  void AddChildSceneNode(SceneNodeData &root_node, TextureManager &texture_manager,
                         SceneNode &parent);

  uint32_t AddObject(PrimitiveData &mesh_data, Material material);

  uint32_t _AddObject(AssetData &asset_data, Material material);

  uint32_t AddInstance(Instance &instance);

  void UpdateInstance(glm::mat4 new_matrix, uint32_t index);

  void UpdateInstances();

  void RecreateTopLevelAS();

  void Serialize(std::filesystem::path file_path);

  void Deserialize(TextureManager &texture_manager, std::filesystem::path file_path);

  void Destroy();
};
