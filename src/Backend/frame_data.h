#pragma once
#include "Backend/context.h"

struct FrameData {
  VkCommandPool command_pool;
  VkCommandBuffer command_buffer;
  VkSemaphore swapchain_semaphore;
  VkSemaphore render_semaphore;
  VkFence render_fence;
};

void CreateFrameData(FrameData &frame_data);

void DestroyFrameData(FrameData &frame_data);
