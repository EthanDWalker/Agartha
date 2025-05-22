#pragma once

#include "Backend/buffer.h"
#include "Backend/context.h"
#include "Backend/pipeline.h"
#include "mesh.h"

const uint32_t MAX_OBJECT_INSTANCES = 1000;

struct Object {
  Mesh mesh;
  AllocatedBuffer instance_buffer;
  std::vector<glm::mat4> instance_matrices;
  VkDeviceAddress instance_buffer_address;
};

struct ObjectPushConstantData {
  VkDeviceAddress vertex_buffer_address;
  VkDeviceAddress instance_buffer_address;
};

void CreateObject(VulkanContext &context, ImmediateSubmit &immediate_submit,
                  MeshData &mesh_data, Object &object);

void AddObjectInstanceMatrix(VulkanContext &context,
                             ImmediateSubmit &immediate_submit,
                             glm::mat4 matrix, Object &object);

void DrawObject(VkCommandBuffer cmd, Pipeline pipeline, Object &object);

void DestroyObject(VulkanContext &context, Object &object);
