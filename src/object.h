#pragma once

#include "Backend/buffer.h"
#include "Backend/context.h"
#include "Backend/pipeline.h"
#include "Managers/instance_manager.h"
#include "Managers/texture_manager.h"
#include "mesh.h"

struct Object {
  Mesh mesh;
  AllocatedBuffer instance_indices_buffer;
  std::vector<uint32_t> instance_indices;
  Material material;
  VkDeviceAddress instance_indices_address;
};

struct ObjectPushConstantData {
  VkDeviceAddress vertex_buffer_address;
  VkDeviceAddress instance_indices_address;
  Material material;
};

void CreateObjectMaterial(VulkanContext &context,
                          ImmediateSubmit &immediate_submit,
                          TextureManager &texture_manager, MeshData &mesh_data,
                          Object &object);

void CreateObject(VulkanContext &context, ImmediateSubmit &immediate_submit,
                  MeshData &mesh_data, Object &object);

void AddObjectInstanceMatrix(VulkanContext &context,
                             ImmediateSubmit &immediate_submit,
                             InstanceManager &instance_manager,
                             glm::mat4 matrix, Object &object);

void DrawObject(VkCommandBuffer cmd, Pipeline pipeline, Object &object);

void DestroyObject(VulkanContext &context, Object &object);
