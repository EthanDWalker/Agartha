#include "scene_svo.h"
#include "Backend/allocated_image.h"
#include "Backend/buffer.h"
#include "Backend/context.h"
#include "Backend/init.h"
#include "Backend/pipeline.h"
#include "Managers/light_manager.h"
#include "Managers/texture_manager.h"
#include "camera.h"
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>

struct SvoData {
  glm::vec3 left_bound;
  float _p0;
  float world_to_svo;
  float voxel_size;
  float voxel_size_diag;
  float inv_voxel_size;
};

void SceneSvo::Create(VulkanContext &context, SceneManager &scene_manager,
                      LightManager &light_manager,
                      TextureManager &texture_manager, Camera &camera,
                      DescriptorBuilder &descriptor_builder) {
  VkExtent3D radiance_image_extent = {
      static_cast<uint32_t>(SVO_EXTENT.width / VOXEL_SIZE),
      static_cast<uint32_t>(SVO_EXTENT.height / VOXEL_SIZE),
      static_cast<uint32_t>(SVO_EXTENT.depth / VOXEL_SIZE),
  };

  CreateAllocatedImage(context, radiance_image_extent,
                       VK_FORMAT_R16G16B16A16_SFLOAT,
                       VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                       radiance_image, true);

  CreateBuffer(context, sizeof(uint32_t), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
               VMA_MEMORY_USAGE_AUTO, draw_count_buffer);

  CreateBuffer(
      context, sizeof(VkDrawIndexedIndirectCommand) * SCENE_MAX_INSTANCES,
      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
      VMA_MEMORY_USAGE_GPU_ONLY, draw_buffer);

  SvoData svo_data{};
  svo_data.left_bound = glm::vec3(
      SVO_EXTENT.width / 2.0, SVO_EXTENT.height / 2.0, SVO_EXTENT.depth / 2.0);
  const uint32_t max_length =
      std::max(std::max(SVO_EXTENT.width, SVO_EXTENT.height), SVO_EXTENT.depth);
  svo_data.world_to_svo = 2.0 / float(max_length);

  svo_data.voxel_size = VOXEL_SIZE;
  svo_data.voxel_size_diag =
      std::sqrt(2.0 * (svo_data.voxel_size * svo_data.voxel_size));
  svo_data.inv_voxel_size = 1 / VOXEL_SIZE;

  CreateBufferDataAsync(context, &svo_data, sizeof(SvoData),
                        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, data_buffer);

  radiance_image_views.resize(CalculateMipLevels(radiance_image.extent));

  for (uint32_t i = 0; i < radiance_image_views.size(); i++) {
    VkImageViewCreateInfo image_view_ci =
        vkinit::ImageViewCI(radiance_image.format, VK_IMAGE_ASPECT_COLOR_BIT,
                            radiance_image.image, 1);
    image_view_ci.subresourceRange.baseMipLevel = i;
    image_view_ci.viewType = VK_IMAGE_VIEW_TYPE_3D;
    vkCreateImageView(context.device, &image_view_ci, nullptr,
                      &radiance_image_views[i]);
  }

  VkSamplerCreateInfo sampler_ci{};
  sampler_ci.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  sampler_ci.magFilter = VK_FILTER_LINEAR;
  sampler_ci.minFilter = VK_FILTER_LINEAR;
  sampler_ci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
  sampler_ci.maxLod = static_cast<float>(radiance_image_views.size());

  vkCreateSampler(context.device, &sampler_ci, nullptr, &radiance_sampler);

  {
    descriptor_builder.BindStorageImages(0, radiance_image_views);
    descriptor_builder.BindUniformBuffer(1, data_buffer.buffer);
    descriptor_builder.BindCombinedImage(2, radiance_image.image_view,
                                         radiance_sampler);
    descriptor_builder.Build(
        context,
        VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT |
            VK_SHADER_STAGE_GEOMETRY_BIT | VK_SHADER_STAGE_COMPUTE_BIT,
        svo_descriptor_set, svo_descriptor_layout);
  }

  {
    descriptor_builder.BindStorageBuffer(0, draw_buffer.buffer);
    descriptor_builder.BindStorageBuffer(1, draw_count_buffer.buffer);
    descriptor_builder.Build(context, VK_SHADER_STAGE_COMPUTE_BIT,
                             draw_buffer_descriptor_set,
                             draw_buffer_descriptor_layout);
    ComputePipelineBuilder pipeline_builder{};
    pipeline_builder.SetShader(context, "VoxelGI/voxelize.comp.spv");
    pipeline_builder.AddDescriptorSetLayout(draw_buffer_descriptor_layout);
    pipeline_builder.AddDescriptorSetLayout(
        scene_manager.instance_descriptor_layout);
    pipeline_builder.AddDescriptorSetLayout(
        scene_manager.object_descriptor_layout);
    pipeline_builder.Build(context, build_draw_buffer_pipeline);
  }

  {
    GraphicsPipelineBuilder pipeline_builder{};
    pipeline_builder.SetShaders(context, "VoxelGI/voxelize.vert.spv",
                                "VoxelGI/voxelize.frag.spv",
                                "VoxelGI/voxelize.geom.spv");
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
    pipeline_builder.SetShaders(context, "VoxelGI/image_debug.vert.spv",
                                "VoxelGI/image_debug.frag.spv",
                                "VoxelGI/image_debug.geom.spv");
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
    ComputePipelineBuilder pipeline_builder{};
    pipeline_builder.SetShader(context, "VoxelGI/mip_map_radiance.comp.spv");
    pipeline_builder.AddDescriptorSetLayout(svo_descriptor_layout);
    pipeline_builder.AddPushConstantRange(sizeof(uint32_t));
    pipeline_builder.Build(context, mip_pipeline);
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

  TransitionImage(cmd, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
                  radiance_image.image);

  VkClearColorValue clear_color_value{};
  clear_color_value = {0.0f, 0.0f, 0.0f, 0.0f};

  VkImageSubresourceRange range{};
  range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  range.layerCount = VK_REMAINING_ARRAY_LAYERS;
  range.levelCount = VK_REMAINING_MIP_LEVELS;

  vkCmdClearColorImage(cmd, radiance_image.image, VK_IMAGE_LAYOUT_GENERAL,
                       &clear_color_value, 1, &range);

  VkViewport viewport = vkinit::Viewport(radiance_image.extent);
  vkCmdSetViewport(cmd, 0, 1, &viewport);

  VkRect2D scissor = vkinit::Scissor(radiance_image.extent);
  vkCmdSetScissor(cmd, 0, 1, &scissor);

  VkRenderingInfo rendering_info =
      vkinit::RenderingInfo(radiance_image.extent, {}, nullptr);
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

  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, mip_pipeline.obj);

  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                          mip_pipeline.layout, 0, 1, &svo_descriptor_set, 0,
                          nullptr);

  GenerateMipmaps(cmd, radiance_image);
}

void SceneSvo::DrawDebugView(VkCommandBuffer cmd, Camera &camera,
                             AllocatedImage &draw_image,
                             AllocatedImage &depth_image,
                             VkImageLayout new_layout, uint32_t mip_level) {
  const Pipeline pipeline = debug_pipeline;
  const uint32_t point_count = std::pow(
      radiance_image.extent.depth / float(VOXEL_SIZE * std::pow(2, mip_level)),
      3);

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

  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.obj);

  std::array<VkDescriptorSet, 2> ds = {
      svo_descriptor_set,
      camera.descriptor_set,
  };

  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.layout,
                          0, ds.size(), ds.data(), 0, nullptr);

  vkCmdPushConstants(cmd, pipeline.layout,
                     VK_SHADER_STAGE_GEOMETRY_BIT | VK_SHADER_STAGE_VERTEX_BIT,
                     0, sizeof(uint32_t), &mip_level);

  vkCmdDraw(cmd, point_count, 1, 0, 0);

  vkCmdEndRendering(cmd);

  TransitionImage(cmd, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, new_layout,
                  draw_image.image);
}

void SceneSvo::Destroy(VulkanContext &context) {
  for (auto &image_view : radiance_image_views) {
    vkDestroyImageView(context.device, image_view, nullptr);
  }
  DestroyAllocatedImage(context, radiance_image);

  DestroyBuffer(context, draw_buffer);
  DestroyBuffer(context, draw_count_buffer);
  DestroyBuffer(context, data_buffer);

  vkDestroyDescriptorSetLayout(context.device, draw_buffer_descriptor_layout,
                               nullptr);
  vkDestroyDescriptorSetLayout(context.device, svo_descriptor_layout, nullptr);

  DestroyImageSampler(context, radiance_sampler);

  DestroyPipeline(context, build_draw_buffer_pipeline);
  DestroyPipeline(context, build_pipeline);
  DestroyPipeline(context, debug_pipeline);
  DestroyPipeline(context, mip_pipeline);
}
