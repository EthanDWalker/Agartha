#include "instance_manager.h"
#include "Backend/buffer.h"
#include "Backend/context.h"
#include "Backend/descriptors.h"
#include "Backend/immediate_submit.h"
#include <glm/mat4x4.hpp>

void InstanceManager::Init(VulkanContext &context,
                           DescriptorBuilder &descriptor_builder) {
  CreateBuffer(context, sizeof(glm::mat4) * MAX_INSTANCES,
               VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                   VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
               VMA_MEMORY_USAGE_GPU_ONLY, buffer);

  descriptor_builder.Reset();
  descriptor_builder.BindStorageBuffer(0, buffer.buffer);
  descriptor_builder.Build(context, VK_SHADER_STAGE_ALL, descriptor_set,
                           descriptor_layout);
}

uint32_t InstanceManager::AddInstance(VulkanContext &context,
                                      ImmediateSubmit &immediate_submit,
                                      glm::mat4 instance) {
  UpdateBuffer(context, immediate_submit, &instance, sizeof(glm::mat4),
               sizeof(glm::mat4) * size, buffer);

  return size++;
}

void InstanceManager::EditInstance(VulkanContext &context,
                                   ImmediateSubmit &immediate_submit,
                                   uint32_t index, glm::mat4 new_instance) {
  UpdateBuffer(context, immediate_submit, &new_instance, sizeof(glm::mat4),
               sizeof(glm::mat4) * index, buffer);
}

void InstanceManager::Destroy(VulkanContext &context) {
  DestroyBuffer(context, buffer);
  vkDestroyDescriptorSetLayout(context.device, descriptor_layout, nullptr);
}
