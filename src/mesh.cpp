#include "mesh.h"
#include "Backend/buffer.h"
#include "Backend/context.h"
#include "fmt/base.h"
#include "types.h"
#include <cassert>
#include <cstdint>
#include <cstring>
#include <fmt/core.h>
#include <vector>

void CreateMesh(VulkanContext &context, ImmediateSubmit immediate_submit,
                MeshData &mesh_data, Mesh &mesh) {
  const size_t vertex_buffer_size = mesh_data.vertices.size() * sizeof(Vertex);
  const size_t index_buffer_size = mesh_data.indices.size() * sizeof(uint32_t);

  CreateBufferData(context, immediate_submit, mesh_data.vertices.data(),
                   vertex_buffer_size,
                   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                       VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                   mesh.vertex_buffer);

  VkBufferDeviceAddressInfo device_address_info{};
  device_address_info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
  device_address_info.buffer = mesh.vertex_buffer.buffer;

  mesh.vertex_address =
      vkGetBufferDeviceAddress(context.device, &device_address_info);

  CreateBufferData(context, immediate_submit, mesh_data.indices.data(),
                   index_buffer_size, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                   mesh.index_buffer);
}

void DestroyMesh(VulkanContext &context, Mesh &mesh) {
  DestroyBuffer(context, mesh.vertex_buffer);
  DestroyBuffer(context, mesh.index_buffer);
}
