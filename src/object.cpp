#include "object.h"
#include "Backend/buffer.h"
#include "Backend/context.h"
#include "Backend/descriptors.h"
#include "Backend/immediate_submit.h"
#include "Backend/pipeline.h"
#include "mesh.h"

void CreateObject(VulkanContext &context, ImmediateSubmit &immediate_submit,
                  DescriptorBuilder &descriptor_builder, MeshData &mesh_data,
                  Object &object) {
  CreateMesh(context, immediate_submit, mesh_data, object.mesh);

  CreateBuffer(context, sizeof(glm::mat4) * MAX_OBJECT_INSTANCES,
               VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                   VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
               VMA_MEMORY_USAGE_GPU_ONLY, object.instance_buffer);

  VkBufferDeviceAddressInfo device_address_info{};
  device_address_info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
  device_address_info.buffer = object.instance_buffer.buffer;

  object.instance_buffer_address =
      vkGetBufferDeviceAddress(context.device, &device_address_info);
}

void AddObjectInstanceMatrix(VulkanContext &context,
                             ImmediateSubmit &immediate_submit,
                             glm::mat4 matrix, Object &object) {
  UpdateBuffer(context, immediate_submit, &matrix, sizeof(glm::mat4),
               sizeof(glm::mat4) * object.instance_matrices.size(),
               object.instance_buffer);

  object.instance_matrices.push_back(matrix);
}

void DrawObject(VkCommandBuffer cmd, Pipeline pipeline, Object &object) {
  vkCmdBindIndexBuffer(cmd, object.mesh.index_buffer.buffer, 0,
                       VK_INDEX_TYPE_UINT32);

  ObjectPushConstantData pc{};
  pc.instance_buffer_address = object.instance_buffer_address;
  pc.vertex_buffer_address = object.mesh.vertex_address;

  vkCmdPushConstants(cmd, pipeline.layout,
                     VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT,
                     0, sizeof(ObjectPushConstantData), &pc);

  vkCmdDrawIndexed(cmd, object.mesh.index_buffer.info.size / sizeof(uint32_t),
                   object.instance_matrices.size(), 0, 0, 0);
}

void DestroyObject(VulkanContext &context, Object &object) {
  DestroyMesh(context, object.mesh);
  DestroyBuffer(context, object.instance_buffer);
}
