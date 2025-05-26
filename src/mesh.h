#pragma once

#include "Backend/buffer.h"
#include "Backend/context.h"
#include "Backend/immediate_submit.h"
#include "Loaders/model.h"
#include <vulkan/vulkan.h>

struct Mesh {
  AllocatedBuffer vertex_buffer;
  AllocatedBuffer index_buffer;
  VkDeviceAddress vertex_address;
};

void CreateMesh(VulkanContext &context, ImmediateSubmit immediate_submit,
                MeshData &mesh_data, Mesh &mesh);

void DestroyMesh(VulkanContext &context, Mesh &mesh);
