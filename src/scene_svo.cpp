#include "scene_svo.h"
#include "Backend/buffer.h"
#include "Backend/context.h"
#include "Backend/init.h"
#include "Backend/pipeline.h"
#include "Backend/util.h"
#include "Managers/light_manager.h"
#include "Managers/texture_manager.h"
#include "camera.h"
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>

struct SvoData {
  glm::vec3 left_bound;
  float world_to_svo;
  glm::vec3 right_bound;
  uint32_t depth;
};

void SceneSvo::Create(VulkanContext &context, SceneManager &scene_manager,
                      LightManager &light_manager,
                      TextureManager &texture_manager, Camera &camera,
                      DescriptorBuilder &descriptor_builder) {
  size_t size = 8;
  for (uint32_t i = 0; i < SVO_DEPTH; i++) {
    CreateBuffer(context, sizeof(SvoNode) * size,
                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                     VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                     VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                 VMA_MEMORY_USAGE_GPU_ONLY, levels[i]);
    size *= 8;
  }

  VkDeviceAddress level_addresses[SVO_DEPTH];
  for (uint32_t i = 0; i < SVO_DEPTH; i++) {
    level_addresses[i] = GetDeviceAddress(context, levels[i].buffer);
  }

  CreateBufferDataAsync(context, level_addresses,
                        sizeof(VkDeviceAddress) * SVO_DEPTH,
                        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, svo_buffer);

  CreateBuffer(context, sizeof(uint32_t), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
               VMA_MEMORY_USAGE_AUTO, draw_count_buffer);

  CreateBuffer(
      context, sizeof(VkDrawIndexedIndirectCommand) * SCENE_MAX_INSTANCES,
      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
      VMA_MEMORY_USAGE_GPU_ONLY, draw_buffer);

  SvoData svo_data{};
  svo_data.left_bound = glm::vec3(
      SVO_EXTENT.width / 2.0, SVO_EXTENT.height / 2.0, SVO_EXTENT.depth / 2.0);
  svo_data.world_to_svo =
      2.0 /
      std::max(std::max(SVO_EXTENT.width, SVO_EXTENT.height), SVO_EXTENT.depth);
  svo_data.right_bound = -svo_data.left_bound;
  svo_data.depth = SVO_DEPTH;
  CreateBufferDataAsync(context, &svo_data, sizeof(SvoData),
                        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, data_buffer);

  {
    descriptor_builder.BindStorageBuffer(0, draw_buffer.buffer);
    descriptor_builder.BindStorageBuffer(1, draw_count_buffer.buffer);
    descriptor_builder.Build(context, VK_SHADER_STAGE_COMPUTE_BIT,
                             draw_buffer_descriptor_set,
                             draw_buffer_descriptor_layout);
    ComputePipelineBuilder pipeline_builder{};
    pipeline_builder.SetShader(context, "VoxelGI/build_svo.comp.spv");
    pipeline_builder.AddDescriptorSetLayout(draw_buffer_descriptor_layout);
    pipeline_builder.AddDescriptorSetLayout(
        scene_manager.instance_descriptor_layout);
    pipeline_builder.AddDescriptorSetLayout(
        scene_manager.object_descriptor_layout);
    pipeline_builder.Build(context, build_draw_buffer_pipeline);
  }

  {
    descriptor_builder.BindStorageBuffer(0, svo_buffer.buffer);
    descriptor_builder.BindUniformBuffer(1, data_buffer.buffer);
    descriptor_builder.Build(context,
                             VK_SHADER_STAGE_FRAGMENT_BIT |
                                 VK_SHADER_STAGE_VERTEX_BIT |
                                 VK_SHADER_STAGE_GEOMETRY_BIT,
                             svo_descriptor_set, svo_descriptor_layout);
  }

  {
    GraphicsPipelineBuilder pipeline_builder{};
    pipeline_builder.SetShaders(context, "VoxelGI/build_svo.vert.spv",
                                "VoxelGI/build_svo.frag.spv",
                                "VoxelGI/build_svo.geom.spv");
    pipeline_builder.Default();
    pipeline_builder.SetNoDepthTest();
    pipeline_builder.SetCullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
    pipeline_builder.SetMultisampling(VK_SAMPLE_COUNT_8_BIT);
    pipeline_builder.AddDescriptorSetLayout(svo_descriptor_layout);
    pipeline_builder.AddDescriptorSetLayout(
        scene_manager.instance_descriptor_layout);
    pipeline_builder.AddDescriptorSetLayout(
        scene_manager.object_descriptor_layout);
    pipeline_builder.AddDescriptorSetLayout(
        texture_manager.descriptor_set_layout);
    pipeline_builder.AddDescriptorSetLayout(
        light_manager.light_descriptor_layout);
    pipeline_builder.AddDescriptorSetLayout(
        light_manager.shadow_descriptor_layout);
    pipeline_builder.AddDescriptorSetLayout(camera.descriptor_layout);
    pipeline_builder.Build(context, build_pipeline);
  }

  {
    GraphicsPipelineBuilder pipeline_builder{};
    pipeline_builder.SetShaders(context, "VoxelGI/voxel_debug.vert.spv",
                                "VoxelGI/voxel_debug.frag.spv",
                                "VoxelGI/voxel_debug.geom.spv");
    pipeline_builder.Default();
    pipeline_builder.AddColorAttachment(VK_FORMAT_R16G16B16A16_SFLOAT);
    pipeline_builder.SetInputTopology(VK_PRIMITIVE_TOPOLOGY_POINT_LIST);
    pipeline_builder.SetCullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
    pipeline_builder.AddDescriptorSetLayout(svo_descriptor_layout);
    pipeline_builder.AddDescriptorSetLayout(camera.descriptor_layout);
    pipeline_builder.AddPushConstantRange(VK_SHADER_STAGE_VERTEX_BIT |
                                              VK_SHADER_STAGE_GEOMETRY_BIT,
                                          sizeof(uint32_t));
    pipeline_builder.Build(context, debug_pipeline);
  }

  {
    GraphicsPipelineBuilder pipeline_builder{};
    pipeline_builder.SetShaders(context, "VoxelGI/voxel_color_debug.vert.spv",
                                "VoxelGI/voxel_color_debug.frag.spv");
    pipeline_builder.Default();
    pipeline_builder.AddColorAttachment(VK_FORMAT_R16G16B16A16_SFLOAT);
    pipeline_builder.AddDescriptorSetLayout(svo_descriptor_layout);
    pipeline_builder.AddDescriptorSetLayout(
        scene_manager.instance_descriptor_layout);
    pipeline_builder.AddDescriptorSetLayout(
        scene_manager.object_descriptor_layout);
    pipeline_builder.AddDescriptorSetLayout(camera.descriptor_layout);
    pipeline_builder.Build(context, color_debug_pipeline);
  }
}

void SceneSvo::BuildDrawCommands(VkCommandBuffer cmd,
                                 SceneManager &scene_manager) {
  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                    build_draw_buffer_pipeline.obj);

  std::array<VkDescriptorSet, 3> ds = {
      draw_buffer_descriptor_set,
      scene_manager.instance_descriptor_set,
      scene_manager.object_descriptor_set,
  };

  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                          build_draw_buffer_pipeline.layout, 0, ds.size(),
                          ds.data(), 0, nullptr);

  if (scene_manager.instance_index == 0) {
    return;
  }

  vkCmdDispatch(cmd, std::ceil(scene_manager.instance_index / 64.0f), 1, 1);
}

void SceneSvo::Build(VkCommandBuffer cmd, SceneManager &scene_manager,
                     TextureManager &texture_manager,
                     LightManager &light_manager, Camera &camera) {
  uint32_t draw_count;
  memcpy(&draw_count, draw_count_buffer.info.pMappedData, sizeof(uint32_t));
  if (draw_count == 0 || draw_count > scene_manager.instance_index) {
    return;
  }

  for (uint32_t i = 0; i < SVO_DEPTH; i++) {
    vkCmdFillBuffer(cmd, levels[i].buffer, 0, levels[i].info.size, 0);
  }

  VkViewport viewport = vkinit::Viewport(SVO_EXTENT);
  vkCmdSetViewport(cmd, 0, 1, &viewport);

  VkRect2D scissor = vkinit::Scissor(SVO_EXTENT);
  vkCmdSetScissor(cmd, 0, 1, &scissor);

  VkRenderingInfo rendering_info =
      vkinit::RenderingInfo(SVO_EXTENT, {}, nullptr);
  vkCmdBeginRendering(cmd, &rendering_info);

  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, build_pipeline.obj);

  std::array<VkDescriptorSet, 7> ds = {
      svo_descriptor_set,
      scene_manager.instance_descriptor_set,
      scene_manager.object_descriptor_set,
      texture_manager.descriptor_set,
      light_manager.light_descriptor_set,
      light_manager.shadow_descriptor_set,
      camera.descriptor_set,
  };

  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          build_pipeline.layout, 0, ds.size(), ds.data(), 0,
                          nullptr);

  vkCmdBindIndexBuffer(cmd, scene_manager.index_buffer.buffer, 0,
                       VK_INDEX_TYPE_UINT32);

  vkCmdDrawIndexedIndirect(
      cmd, draw_buffer.buffer, 0, draw_count,
      static_cast<uint32_t>(sizeof(VkDrawIndexedIndirectCommand)));

  vkCmdEndRendering(cmd);
}

void SceneSvo::DrawDebugView(VkCommandBuffer cmd, Camera &camera,
                             AllocatedImage &draw_image,
                             AllocatedImage &depth_image,
                             VkImageLayout new_layout) {
  const uint32_t level = SVO_DEPTH;
  TransitionImage(cmd, VK_IMAGE_LAYOUT_UNDEFINED,
                  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, draw_image.image);

  VkViewport viewport = vkinit::Viewport(draw_image.extent);
  vkCmdSetViewport(cmd, 0, 1, &viewport);
  VkRect2D scissor = vkinit::Scissor(draw_image.extent);
  vkCmdSetScissor(cmd, 0, 1, &scissor);

  VkClearColorValue clear_color_value{};
  clear_color_value = {0.0f, 0.0f, 0.0f, 0.0f};

  VkClearValue clear_value{};
  clear_value.color = clear_color_value;

  VkRenderingAttachmentInfo color_att =
      vkinit::AttachmentInfo(draw_image.image_view, nullptr, &clear_value,
                             VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

  std::array<VkRenderingAttachmentInfo, 1> attachments = {
      color_att,
  };

  VkRenderingAttachmentInfo depth_att = vkinit::DepthAttachmentInfo(
      depth_image.image_view, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

  VkRenderingInfo rendering_info =
      vkinit::RenderingInfo(draw_image.extent, attachments, &depth_att);

  vkCmdBeginRendering(cmd, &rendering_info);

  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, debug_pipeline.obj);

  std::array<VkDescriptorSet, 2> ds = {
      svo_descriptor_set,
      camera.descriptor_set,
  };

  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          debug_pipeline.layout, 0, ds.size(), ds.data(), 0,
                          nullptr);

  vkCmdPushConstants(cmd, debug_pipeline.layout,
                     VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT,
                     0, sizeof(uint32_t), &level);

  vkCmdDraw(cmd, std::pow(std::pow(2, level) + 1, 3), 1, 0, 0);

  vkCmdEndRendering(cmd);

  TransitionImage(cmd, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, new_layout,
                  draw_image.image);
}

void SceneSvo::DrawColorDebugView(VkCommandBuffer cmd,
                                  SceneManager &scene_manager, Camera &camera,
                                  AllocatedImage &draw_image,
                                  AllocatedImage &depth_image,
                                  VkImageLayout new_layout) {
  uint32_t draw_count;
  memcpy(&draw_count, draw_count_buffer.info.pMappedData, sizeof(uint32_t));
  if (draw_count == 0) {
    TransitionImage(cmd, VK_IMAGE_LAYOUT_UNDEFINED, new_layout,
                    draw_image.image);
    return;
  }
  TransitionImage(cmd, VK_IMAGE_LAYOUT_UNDEFINED,
                  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, draw_image.image);

  VkViewport viewport = vkinit::Viewport(draw_image.extent);
  vkCmdSetViewport(cmd, 0, 1, &viewport);
  VkRect2D scissor = vkinit::Scissor(draw_image.extent);
  vkCmdSetScissor(cmd, 0, 1, &scissor);

  VkClearColorValue clear_color_value{};
  clear_color_value = {0.0f, 0.0f, 0.0f, 0.0f};

  VkClearValue clear_value{};
  clear_value.color = clear_color_value;

  VkRenderingAttachmentInfo color_att =
      vkinit::AttachmentInfo(draw_image.image_view, nullptr, &clear_value,
                             VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

  std::array<VkRenderingAttachmentInfo, 1> attachments = {
      color_att,
  };

  VkRenderingAttachmentInfo depth_att = vkinit::DepthAttachmentInfo(
      depth_image.image_view, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

  VkRenderingInfo rendering_info =
      vkinit::RenderingInfo(draw_image.extent, attachments, &depth_att);

  vkCmdBeginRendering(cmd, &rendering_info);

  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    color_debug_pipeline.obj);

  std::array<VkDescriptorSet, 4> ds = {
      svo_descriptor_set,
      scene_manager.instance_descriptor_set,
      scene_manager.object_descriptor_set,
      camera.descriptor_set,
  };

  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          color_debug_pipeline.layout, 0, ds.size(), ds.data(),
                          0, nullptr);

  vkCmdDrawIndexedIndirect(
      cmd, draw_buffer.buffer, 0, draw_count,
      static_cast<uint32_t>(sizeof(VkDrawIndexedIndirectCommand)));

  vkCmdEndRendering(cmd);

  TransitionImage(cmd, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, new_layout,
                  draw_image.image);
}

void SceneSvo::Destroy(VulkanContext &context) {
  for (uint32_t i = 0; i < SVO_DEPTH; i++) {
    DestroyBuffer(context, levels[i]);
  }

  DestroyBuffer(context, draw_buffer);
  DestroyBuffer(context, draw_count_buffer);
  DestroyBuffer(context, data_buffer);
  DestroyBuffer(context, svo_buffer);

  vkDestroyDescriptorSetLayout(context.device, draw_buffer_descriptor_layout,
                               nullptr);
  vkDestroyDescriptorSetLayout(context.device, svo_descriptor_layout, nullptr);

  DestroyPipeline(context, build_draw_buffer_pipeline);
  DestroyPipeline(context, build_pipeline);
  DestroyPipeline(context, debug_pipeline);
  DestroyPipeline(context, color_debug_pipeline);
}
