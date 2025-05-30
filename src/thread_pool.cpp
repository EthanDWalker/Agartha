#include "thread_pool.h"
#include "Backend/context.h"
#include "Backend/init.h"
#include "Backend/util.h"
#include "fmt/base.h"
#include <future>
#include <mutex>
#include <thread>
#include <vector>

void ThreadPool::Create(VulkanContext &context) {
  thread_count = std::thread::hardware_concurrency() / 2;
  fmt::println("[ThreadPool] using {} threads", thread_count);

  thread_context.reserve(thread_count);
  threads.reserve(thread_count);

  for (uint32_t i = 0; i < thread_count; i++) {
    ThreadContext thread{};

    VkCommandPoolCreateInfo command_pool_ci{};
    command_pool_ci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    command_pool_ci.queueFamilyIndex = context.graphics_queue_index;
    command_pool_ci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    VK_CHECK(vkCreateCommandPool(context.device, &command_pool_ci, nullptr,
                                 &thread.command_pool));

    VkCommandBufferAllocateInfo command_buffer_ci{};
    command_buffer_ci.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    command_buffer_ci.commandBufferCount = 1;
    command_buffer_ci.commandPool = thread.command_pool;
    command_buffer_ci.level = VK_COMMAND_BUFFER_LEVEL_SECONDARY;

    VK_CHECK(vkAllocateCommandBuffers(context.device, &command_buffer_ci,
                                      &thread.command_buffer));

    thread_context.push_back(thread);
  }

  for (uint32_t i = 0; i < thread_count; i++) {
    threads.emplace_back([this, i]() {
      while (true) {
        std::function<void(VkCommandBuffer)> task;

        {
          std::unique_lock<std::mutex> lock(queue_mutex);
          condition.wait(lock,
                         [this]() { return stop || !task_queue.empty(); });

          if (stop && task_queue.empty()) {
            return;
          }

          task = std::move(task_queue.front());
          task_queue.pop();
        }

        task(thread_context[i].command_buffer);
      }
    });
  }
}

std::future<void>
ThreadPool::RunTask(std::function<void(VkCommandBuffer)> task) {
  std::promise<void> promise;

  std::future<void> future = promise.get_future();

  auto wrapped_task = [&promise, task](VkCommandBuffer s_cmd) {
    task(s_cmd);
    promise.set_value();
  };

  {
    std::unique_lock<std::mutex> lock(queue_mutex);
    task_queue.push(wrapped_task);
  }

  condition.notify_one();

  return future;
}

void ThreadPool::Destroy(VulkanContext &context) {
  {
    std::unique_lock<std::mutex> lock(queue_mutex);
    stop = true;
  }

  condition.notify_all();

  for (std::thread &t : threads) {
    if (t.joinable())
      t.join();
  }

  for (uint32_t i = 0; i < thread_count; i++) {
    vkDestroyCommandPool(context.device, thread_context[i].command_pool,
                         nullptr);
  }
}
