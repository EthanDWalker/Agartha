#pragma once

#include "Backend/acceleration_structure.h"
#include "Backend/buffer.h"
#include "Backend/context.h"
#include "Backend/descriptors.h"
#include "Backend/immediate_submit.h"
#include "Loaders/model.h"
#include "types.h"
#include <glm/vec3.hpp>
#include <queue>
#include <vector>

const uint32_t SCENE_MAX_OBJECTS = 2048;
const uint32_t SCENE_MAX_INSTANCES = 4096;
const uint32_t SCENE_MAX_INDICES = 10000000; // 10 million

struct Object {
  Material material;
};

struct SphereBounds {
  float radius;
};

struct AabbBounds {
  glm::vec4 min;
  glm::vec4 max;
};

struct GpuMesh {
  VkDeviceAddress vertex_address;
  uint32_t first_index;
  uint32_t index_count;
};

struct Instance {
  glm::mat4 matrix;
  uint32_t object_index;
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
  std::vector<glm::mat4> instance_matrices;

  AccelerationStructure top_level_as;

  std::queue<uint32_t> changed_instances;

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

  std::vector<uint32_t> AddObjects(VulkanContext &context,
                                   std::vector<MeshData> data,
                                   std::vector<Material> materials = {});

  uint32_t AddObject(VulkanContext &context, MeshData &data,
                     Material material = {});

  uint32_t AddInstance(VulkanContext &context, Instance &instance);

  void UpdateInstance(glm::mat4 new_matrix, uint32_t index);

  void UpdateInstances(VulkanContext &context);

  void RecreateTopLevelAS(VulkanContext &context);

  void Destroy(VulkanContext &context);
};
