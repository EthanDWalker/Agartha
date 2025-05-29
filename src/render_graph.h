#pragma once

#include "Backend/allocated_image.h"
#include "Backend/buffer.h"
#include "Backend/context.h"
#include "Backend/frame_data.h"
#include "Backend/swapchain.h"
#include <cstdint>
#include <functional>
#include <vector>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

constexpr uint8_t FRAME_OVERLAP = 3;

struct Dependency {
  std::vector<VkBufferMemoryBarrier2> buffer_deps;
  std::vector<VkImageMemoryBarrier2> image_deps;
  std::vector<VkMemoryBarrier2> memory_deps;
};

struct RenderPass {
  Dependency dependencies;
  std::function<void(VkCommandBuffer)> callback;
};

struct DependencyBuilder {
  Dependency dependency;

  void AddDependency(AllocatedImage image, VkAccessFlagBits2 src_access,
                     VkAccessFlagBits2 dst_access,
                     VkPipelineStageFlagBits2 src_stage,
                     VkPipelineStageFlagBits2 dst_stage);
  void AddDependency(AllocatedBuffer buffer, VkAccessFlagBits2 src_access,
                     VkAccessFlagBits2 dst_access,
                     VkPipelineStageFlagBits2 src_stage,
                     VkPipelineStageFlagBits2 dst_stage);
  void AddDependency(VkAccessFlagBits2 src_access, VkAccessFlagBits2 dst_access,
                     VkPipelineStageFlagBits2 src_stage,
                     VkPipelineStageFlagBits2 dst_stage);
};

struct RenderGraphBuilder {
  std::vector<std::vector<RenderPass>> render_graph;

  void AddPass(uint32_t level, Dependency dependency,
               std::function<void(VkCommandBuffer)> &callback);
};

struct RenderGraph {
  std::vector<std::vector<RenderPass>> render_graph;
  Swapchain swapchain;
  AllocatedImage draw_image;
  AllocatedImage depth_image;
  FrameData frame_data[FRAME_OVERLAP];
  size_t frame_number;
  bool resize_requested;

  void Init(VulkanContext &context, GLFWwindow *window);

  void Render(VulkanContext &context);

  void Resize(VulkanContext &context, GLFWwindow *window);

  void Destroy(VulkanContext &context);
};
