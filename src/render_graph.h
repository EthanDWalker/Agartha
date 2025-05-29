#pragma once

#include "Backend/allocated_image.h"
#include "Backend/context.h"
#include "Backend/frame_data.h"
#include "Backend/swapchain.h"
#include <cstdint>
#include <functional>
#include <vector>
#include <vulkan/vulkan.h>

constexpr uint8_t FRAME_OVERLAP = 3;

struct Dependency {
  std::vector<VkBufferMemoryBarrier2> buffer_deps;
  std::vector<VkImageMemoryBarrier2> image_deps;
  std::vector<VkMemoryBarrier2> memory_deps;
  VkDependencyInfo info;
};

struct RenderPass {
  std::vector<RenderPass> parents;
  std::vector<Dependency> dependencies;
  std::function<void(VkCommandBuffer cmd)> callback;
};

struct RenderGraphBuilder {
  std::vector<std::vector<RenderPass>> render_graph;
};

struct RenderGraph {
  RenderPass root;
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
