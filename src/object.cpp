#include "object.h"
#include "Backend/buffer.h"
#include "Backend/context.h"
#include "Backend/immediate_submit.h"
#include "Backend/pipeline.h"
#include "Managers/instance_manager.h"
#include "Managers/texture_manager.h"
#include "mesh.h"
#include <cstdint>

void CreateObject(VulkanContext &context, ImmediateSubmit &immediate_submit,
                  MeshData &mesh_data, Object &object) {
  CreateMesh(context, immediate_submit, mesh_data, object.mesh);
  CreateBuffer(context, sizeof(uint32_t) * 25,
               VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                   VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
               VMA_MEMORY_USAGE_GPU_ONLY, object.instance_indices_buffer);

  VkBufferDeviceAddressInfo address_info{};
  address_info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
  address_info.buffer = object.instance_indices_buffer.buffer;

  object.instance_indices_address =
      vkGetBufferDeviceAddress(context.device, &address_info);
}

void CreateObjectMaterial(VulkanContext &context,
                          ImmediateSubmit &immediate_submit,
                          TextureManager &texture_manager, MeshData &mesh_data,
                          Object &object) {
  CreateObject(context, immediate_submit, mesh_data, object);
  object.material = texture_manager.GetMaterial(mesh_data.material_data);
}

void AddObjectInstanceMatrix(VulkanContext &context,
                             ImmediateSubmit &immediate_submit,
                             InstanceManager &instance_manager,
                             glm::mat4 matrix, Object &object) {
  uint32_t index =
      instance_manager.AddInstance(context, immediate_submit, matrix);

  UpdateBuffer(context, immediate_submit, &index, sizeof(uint32_t),
               sizeof(uint32_t) * object.instance_indices.size(),
               object.instance_indices_buffer);

  object.instance_indices.push_back(index);
}

void DrawObject(VkCommandBuffer cmd, Pipeline pipeline, Object &object) {
  vkCmdBindIndexBuffer(cmd, object.mesh.index_buffer.buffer, 0,
                       VK_INDEX_TYPE_UINT32);

  ObjectPushConstantData pc{};
  pc.material = object.material;
  pc.instance_indices_address = object.instance_indices_address;
  pc.vertex_buffer_address = object.mesh.vertex_address;

  vkCmdPushConstants(cmd, pipeline.layout,
                     VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT,
                     0, sizeof(ObjectPushConstantData), &pc);

  vkCmdDrawIndexed(cmd, object.mesh.index_buffer.info.size / sizeof(uint32_t),
                   object.instance_indices.size(), 0, 0, 0);
}

void DestroyObject(VulkanContext &context, Object &object) {
  DestroyMesh(context, object.mesh);
  DestroyBuffer(context, object.instance_indices_buffer);
}
