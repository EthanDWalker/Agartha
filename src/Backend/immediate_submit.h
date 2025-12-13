#pragma once

#include <functional>
#include <volk.h>

struct ImmediateSubmit {
  struct ThreadData {
    VkCommandPool command_pool;
    VkCommandBuffer command_buffer;
    VkFence fence;
  };

  static thread_local ThreadData thread_data;

  static void Submit(std::function<void(VkCommandBuffer cmd)> &&function);
};
