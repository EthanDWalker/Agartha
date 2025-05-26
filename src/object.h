#pragma once

#include "Backend/buffer.h"
#include "Backend/context.h"
#include "Backend/descriptors.h"
#include "Backend/pipeline.h"
#include "mesh.h"
#include "texture_manager.h"

const uint32_t MAX_OBJECT_INSTANCES = 100;

struct Object {
  Mesh mesh;
  AllocatedBuffer instance_buffer;
  Material material;
  std::vector<glm::mat4> instance_matrices;
  VkDeviceAddress instance_buffer_address;
};

struct ObjectPushConstantData {
  VkDeviceAddress vertex_buffer_address;
  VkDeviceAddress instance_buffer_address;
  Material material;
};

void CreateObjectMaterial(VulkanContext &context,
                          ImmediateSubmit &immediate_submit,
                          DescriptorBuilder &descriptor_builder,
                          TextureManager &texture_manager, MeshData &mesh_data,
                          Object &object);

void CreateObject(VulkanContext &context, ImmediateSubmit &immediate_submit,
                  DescriptorBuilder &descriptor_builder, MeshData &mesh_data,
                  Object &object);

void AddObjectInstanceMatrix(VulkanContext &context,
                             ImmediateSubmit &immediate_submit,
                             glm::mat4 matrix, Object &object);

void DrawObject(VkCommandBuffer cmd, Pipeline pipeline, Object &object);

void DestroyObject(VulkanContext &context, Object &object);
