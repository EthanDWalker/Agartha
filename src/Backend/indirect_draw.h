#pragma once

#include "buffer.h"
#include "descriptors.h"
#include "pipeline.h"

struct IndirectDrawIndexedCommand {
  AllocatedBuffer draw_buffer;
  AllocatedBuffer draw_count_buffer;

  Pipeline build_pipeline;

  VkDescriptorSet draw_descriptor_set;
  VkDescriptorSetLayout draw_descriptor_layout;

  uint32_t max_draw_count;

  void Init(DescriptorBuilder &descriptor_builder,
            std::string shader, uint32_t max_draw_count,
            VkDescriptorSetLayout *descriptor_layouts,
            uint32_t descriptor_layout_count, uint32_t push_constant_range = 0);

  void BuildDraw(VkCommandBuffer cmd, VkDescriptorSet *descriptor_sets,
                 uint32_t descriptor_set_count,
                 std::array<uint32_t, 3> dispatch,
                 uint32_t push_constant_size = 0,
                 void *push_constant_data = nullptr);

  uint32_t GetDrawCount();

  void Draw(VkCommandBuffer cmd);

  void Destroy();
};
