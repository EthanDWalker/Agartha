#pragma once

#include "Backend/buffer.h"
#include "Backend/context.h"
#include "Backend/immediate_submit.h"
#include "Backend/pipeline.h"
#include "types.h"
#include <cstdint>
#include <span>
#include <vulkan/vulkan.h>

struct Mesh {
  AllocatedBuffer vertex_buffer;
  AllocatedBuffer index_buffer;
  VkDeviceAddress vertex_address;
  std::vector<glm::mat4> instance_matrices;
};

void CreateMesh(VulkanContext &context, ImmediateSubmit immediate_submit,
                std::span<uint32_t> indices, std::span<Vertex> vertices,
                Mesh &mesh);

void DrawMesh(VkCommandBuffer cmd, Pipeline &pipeline, glm::mat4 world_matrix,
              glm::vec3 view_pos, Mesh &mesh);

void DestroyMesh(VulkanContext &context, Mesh &mesh);
