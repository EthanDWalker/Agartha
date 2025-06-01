#pragma once

#include "Backend/buffer.h"
#include "Backend/context.h"
#include "Backend/descriptors.h"
#include "Loaders/model.h"
#include "types.h"
#include <glm/vec3.hpp>
#include <queue>
#include <vector>
#include <vulkan/vulkan.h>

const uint32_t SCENE_MAX_OBJECTS = 1000;
const uint32_t SCENE_MAX_INSTANCES = 2000;

struct Object {
  Material material;
};

struct Mesh {
  AllocatedBuffer vertex_buffer;
  AllocatedBuffer index_buffer;
};

struct GpuMesh {
  VkDeviceAddress vertex_address;
  VkDeviceAddress index_address;
  uint32_t index_count;
  float padding;
};

struct SphereBounds {
  glm::vec3 center;
  float radius;
};

struct Instance {
  glm::mat4 matrix;
  glm::vec3 color;
  uint32_t object_index;
};

struct SceneManager {
  AllocatedBuffer object_buffer;
  AllocatedBuffer mesh_buffer;
  AllocatedBuffer sphere_bounds_buffer;
  AllocatedBuffer instance_buffer;

  std::vector<Mesh> meshes;
  std::vector<uint32_t> instance_mesh;

  std::queue<uint32_t> removed_objects;
  std::queue<uint32_t> removed_instances;

  VkDescriptorSet object_descriptor_set;
  VkDescriptorSetLayout object_descriptor_layout;

  VkDescriptorSet instance_descriptor_set;
  VkDescriptorSetLayout instance_descriptor_layout;

  uint32_t object_index;
  uint32_t instance_index;

  void Init(VulkanContext &context, DescriptorBuilder &descriptor_builder);

  uint32_t AddObject(VulkanContext &context, ImmediateSubmit &immediate_submit,
                     MeshData &data, Material &material);

  void RemoveObject(VulkanContext &context, ImmediateSubmit &immediate_submit,
                    uint32_t index);

  uint32_t AddInstance(VulkanContext &context,
                       ImmediateSubmit &immediate_submit, Instance *instance);

  void EditInstance(VulkanContext &context, ImmediateSubmit &immediate_submit,
                    Instance *instance, uint32_t index);

  void RemoveInstance(VulkanContext &context, ImmediateSubmit &immediate_submit,
                      uint32_t index);

  void Destroy(VulkanContext &context);
};
