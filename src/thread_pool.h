#pragma once

#include "Backend/context.h"
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <vector>
#include <vulkan/vulkan.h>

struct ThreadContext {
  VkCommandPool command_pool;
  VkCommandBuffer command_buffer;
};

struct ThreadPool {
  std::vector<ThreadContext> thread_context;
  std::vector<std::thread> threads;
  std::queue<std::function<void(VkCommandBuffer)>> task_queue;

  std::mutex queue_mutex;
  std::condition_variable condition;
  std::atomic<bool> stop;

  uint32_t thread_count;

  void Create(VulkanContext &context);

  std::future<void> RunTask(std::function<void(VkCommandBuffer)>);

  void Destroy(VulkanContext &context);
};
