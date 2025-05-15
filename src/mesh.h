#pragma once

#include "Backend/buffer.h"
#include "Backend/context.h"
#include "Backend/immediate_submit.h"
#include "types.h"
#include <cstdint>
#include <span>
#include <vulkan/vulkan.h>

struct Mesh {
  AllocatedBuffer vertex_buffer;
  AllocatedBuffer index_buffer;
  VkDeviceAddress vertex_address;
};

void CreateMesh(VulkanContext &context, ImmediateSubmit immediate_submit,
                std::span<uint32_t> indices, std::span<Vertex> vertices, Mesh &mesh);

void DestroyMesh(VulkanContext &context, Mesh &mesh);
