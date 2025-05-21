#include "mesh.h"
#include "Backend/buffer.h"
#include "Backend/context.h"
#include "Backend/pipeline.h"
#include "types.h"
#include <cassert>
#include <cstdint>
#include <cstring>
#include <fmt/core.h>
#define VMA_IMPLEMENTATION
#include <vma/vk_mem_alloc.h>

void DrawMesh(VkCommandBuffer cmd, Pipeline &pipeline, Mesh &mesh) {
  vkCmdBindIndexBuffer(cmd, mesh.index_buffer.buffer, 0, VK_INDEX_TYPE_UINT32);

  vkCmdDrawIndexed(cmd, mesh.index_buffer.info.size / sizeof(uint32_t), 1, 0, 0,
                   0);
}

void CreateMesh(VulkanContext &context, ImmediateSubmit immediate_submit,
                MeshData &mesh_data, Mesh &mesh) {
  const size_t vertex_buffer_size = mesh_data.vertices.size() * sizeof(Vertex);
  const size_t index_buffer_size = mesh_data.indices.size() * sizeof(uint32_t);

  CreateBuffer(context, vertex_buffer_size,
               VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                   VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                   VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
               VMA_MEMORY_USAGE_GPU_ONLY, mesh.vertex_buffer);

  VkBufferDeviceAddressInfo device_address_info{};
  device_address_info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
  device_address_info.buffer = mesh.vertex_buffer.buffer;

  mesh.vertex_address =
      vkGetBufferDeviceAddress(context.device, &device_address_info);

  CreateBuffer(context, index_buffer_size,
               VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                   VK_BUFFER_USAGE_TRANSFER_DST_BIT,
               VMA_MEMORY_USAGE_GPU_ONLY, mesh.index_buffer);

  AllocatedBuffer staging_buffer;

  CreateBuffer(context, vertex_buffer_size + index_buffer_size,
               VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY,
               staging_buffer);

  void *data = staging_buffer.allocation->GetMappedData();

  assert(staging_buffer.allocation != nullptr);

  memcpy(data, mesh_data.vertices.data(), vertex_buffer_size);

  memcpy((char *)data + vertex_buffer_size, mesh_data.indices.data(),
         index_buffer_size);

  immediate_submit.Submit(context, [&](VkCommandBuffer cmd) {
    VkBufferCopy vertex_copy{0};
    vertex_copy.dstOffset = 0;
    vertex_copy.srcOffset = 0;
    vertex_copy.size = vertex_buffer_size;

    vkCmdCopyBuffer(cmd, staging_buffer.buffer, mesh.vertex_buffer.buffer, 1,
                    &vertex_copy);

    VkBufferCopy index_copy{0};
    index_copy.srcOffset = vertex_buffer_size;
    index_copy.dstOffset = 0;
    index_copy.size = index_buffer_size;

    vkCmdCopyBuffer(cmd, staging_buffer.buffer, mesh.index_buffer.buffer, 1,
                    &index_copy);
  });

  DestroyBuffer(context, staging_buffer);
}

void DestroyMesh(VulkanContext &context, Mesh &mesh) {
  DestroyBuffer(context, mesh.vertex_buffer);
  DestroyBuffer(context, mesh.index_buffer);
}
