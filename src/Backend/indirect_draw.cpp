#include "indirect_draw.h"
#include "buffer.h"
#include "context.h"
#include "descriptors.h"
#include "pipeline.h"
#include <cstring>

void IndirectDrawIndexedCommand::Init(VulkanContext &context,
                                      DescriptorBuilder &descriptor_builder,
                                      std::string shader,
                                      uint32_t max_draw_count,
                                      VkDescriptorSetLayout *descriptor_layouts,
                                      uint32_t descriptor_layout_count) {
  CreateBuffer(context, sizeof(VkDrawIndexedIndirectCommand) * max_draw_count,
               VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                   VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
               VMA_MEMORY_USAGE_GPU_ONLY, draw_buffer);
  CreateBuffer(context, sizeof(uint32_t), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
               VMA_MEMORY_USAGE_AUTO, draw_count_buffer);

  descriptor_builder.BindStorageBuffer(0, draw_buffer.buffer);
  descriptor_builder.BindStorageBuffer(1, draw_count_buffer.buffer);
  descriptor_builder.Build(context, VK_SHADER_STAGE_COMPUTE_BIT,
                           draw_descriptor_set, draw_descriptor_layout);

  ComputePipelineBuilder pipeline_builder{};
  pipeline_builder.SetShader(context, shader);
  pipeline_builder.AddDescriptorSetLayout(draw_descriptor_layout);
  for (uint32_t i = 0; i < descriptor_layout_count; i++) {
    pipeline_builder.AddDescriptorSetLayout(descriptor_layouts[i]);
  }
  pipeline_builder.Build(context, build_pipeline);

  this->max_draw_count = max_draw_count;
}

void IndirectDrawIndexedCommand::BuildDraw(VkCommandBuffer cmd,
                                           VkDescriptorSet *descriptor_sets,
                                           uint32_t descriptor_set_count,
                                           std::array<uint32_t, 3> dispatch) {
  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, build_pipeline.obj);

  std::vector<VkDescriptorSet> ds;
  ds.reserve(descriptor_set_count + 1);
  ds.push_back(draw_descriptor_set);
  for (uint32_t i = 0; i < descriptor_set_count; i++) {
    ds.push_back(descriptor_sets[i]);
  }

  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                          build_pipeline.layout, 0, ds.size(), ds.data(), 0,
                          nullptr);

  vkCmdDispatch(cmd, dispatch[0], dispatch[1], dispatch[2]);
}

uint32_t IndirectDrawIndexedCommand::GetDrawCount() {
  uint32_t draw_count = 0;
  memcpy(&draw_count, draw_count_buffer.info.pMappedData, sizeof(uint32_t));
  return draw_count;
}

void IndirectDrawIndexedCommand::Draw(VkCommandBuffer cmd) {
  uint32_t draw_count = 0;
  memcpy(&draw_count, draw_count_buffer.info.pMappedData, sizeof(uint32_t));
  if (draw_count == 0 || draw_count >= max_draw_count)
    return;
  vkCmdDrawIndexedIndirect(
      cmd, draw_buffer.buffer, 0, draw_count,
      static_cast<uint32_t>(sizeof(VkDrawIndexedIndirectCommand)));
}

void IndirectDrawIndexedCommand::Destroy(VulkanContext &context) {
  DestroyBuffer(context, draw_buffer);
  DestroyBuffer(context, draw_count_buffer);

  DestroyPipeline(context, build_pipeline);

  vkDestroyDescriptorSetLayout(context.device, draw_descriptor_layout, nullptr);
}
