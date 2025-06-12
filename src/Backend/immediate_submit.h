#pragma once

#include "Backend/context.h"
#include <functional>

struct ImmediateSubmit {
  VkCommandPool command_pool;
  VkCommandBuffer command_buffer;
  VkFence fence;

  void Create(VulkanContext &context);

  void Submit(VulkanContext &context,
              std::function<void(VkCommandBuffer cmd)> &&function);

  static void SubmitAsync(VulkanContext &context,
                          std::function<void(VkCommandBuffer cmd)> &&function);

  void Destroy(VulkanContext &context);
};
