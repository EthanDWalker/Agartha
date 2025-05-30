#pragma once

#include "Backend/buffer.h"
#include "Backend/context.h"
#include "Backend/descriptors.h"
#include <cstdint>
#include <glm/mat4x4.hpp>
#include <vulkan/vulkan.h>

struct InstanceManager {
  AllocatedBuffer buffer;
  VkDescriptorSet descriptor_set;
  VkDescriptorSetLayout descriptor_layout;
  uint32_t size;
  const uint32_t MAX_INSTANCES = 1024;

  void Init(VulkanContext &context, DescriptorBuilder &descriptor_builder);

  uint32_t AddInstance(VulkanContext &context,
                       ImmediateSubmit &immediate_submit, glm::mat4 instance);

  void EditInstance(VulkanContext &context, ImmediateSubmit &immediate_submit,
                    uint32_t index, glm::mat4 new_instance);

  void Destroy(VulkanContext &context);
};
