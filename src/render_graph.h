#pragma once

#include "Backend/allocated_image.h"
#include "Backend/buffer.h"
#include "Backend/frame_data.h"
#include "Backend/swapchain.h"
#include <cstdint>
#include <functional>
#include <vector>

constexpr uint8_t FRAME_OVERLAP = 3;

struct Dependency {
  std::vector<VkBufferMemoryBarrier2> buffer_deps;
  std::vector<VkImageMemoryBarrier2> image_deps;
  std::vector<VkMemoryBarrier2> memory_deps;
};

struct RenderPass {
  Dependency dependency;
  std::function<void(VkCommandBuffer)> callback;
  bool *condition;
};

struct DependencyBuilder {
  Dependency dependency;

  void AddImageDependency(AllocatedImage image, VkAccessFlagBits2 src_access,
                          VkAccessFlagBits2 dst_access, VkPipelineStageFlagBits2 src_stage,
                          VkPipelineStageFlagBits2 dst_stage, VkImageLayout old_layout,
                          VkImageLayout new_layout, bool depth = false);

  void AddBufferDependency(AllocatedBuffer buffer, VkAccessFlagBits2 src_access,
                           VkAccessFlagBits2 dst_access, VkPipelineStageFlagBits2 src_stage,
                           VkPipelineStageFlagBits2 dst_stage);

  void AddDependency(VkAccessFlagBits2 src_access, VkAccessFlagBits2 dst_access,
                     VkPipelineStageFlagBits2 src_stage, VkPipelineStageFlagBits2 dst_stage);
};

struct RenderGraphBuilder {
  std::vector<std::vector<RenderPass>> render_graph;

  void AddPass(uint32_t level, Dependency dependency, std::function<void(VkCommandBuffer)> callback,
               bool *condition = nullptr);
};

struct RenderGraph {
  std::vector<std::vector<RenderPass>> render_graph;
  Dependency root_dep;
  std::function<void(VkCommandBuffer, VkImage, VkImageView, VkExtent2D)> root_callback;
  Swapchain swapchain;
  FrameData frame_data[FRAME_OVERLAP];
  size_t frame_number;
  bool resize_requested;

  void Init(GLFWwindow *window);

  void Render();

  void Resize(GLFWwindow *window);

  void Destroy();
};
