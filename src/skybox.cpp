#include "skybox.h"

#include "Backend/allocated_image.h"
#include "Backend/context.h"
#include "Backend/descriptors.h"
#include "Backend/immediate_submit.h"
#include "Backend/init.h"
#include "Backend/pipeline.h"
#include "Backend/util.h"
#include "Managers/texture_manager.h"
#include <cstdint>
#include <vulkan/vulkan.h>

void GenerateBrdfLut(VulkanContext &context, ImmediateSubmit &immediate_submit,
                     DescriptorBuilder &descriptor_builder, VkSampler sampler,
                     Skybox &skybox) {
  // Must also change resolution in comp shader
  VkExtent3D cube_map_size = {512, 512, 1};

  CreateAllocatedImage(context, cube_map_size, VK_FORMAT_R16G16B16A16_SFLOAT,
                       VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
                       skybox.brdf);

  Pipeline compute_pipeline;
  VkDescriptorSetLayout ds_layout;
  VkDescriptorSet ds;

  descriptor_builder.Reset();
  descriptor_builder.BindStorageImage(0, skybox.brdf.image_view);
  descriptor_builder.Build(context, VK_SHADER_STAGE_COMPUTE_BIT, ds, ds_layout);

  ComputePipelineBuilder pipeline_builder{};
  pipeline_builder.SetShader(context, "brdf.comp.spv");
  pipeline_builder.AddDescriptorSetLayout(ds_layout);
  pipeline_builder.Build(context, compute_pipeline);

  immediate_submit.Submit(context, [&](VkCommandBuffer cmd) {
    TransitionImage(cmd, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
                    skybox.brdf.image);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                      compute_pipeline.obj);

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                            compute_pipeline.layout, 0, 1, &ds, 0, nullptr);

    vkCmdDispatch(cmd, (cube_map_size.width + 15) / 16,
                  (cube_map_size.width + 15) / 16, 6);

    TransitionImage(cmd, VK_IMAGE_LAYOUT_GENERAL,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    skybox.brdf.image);
  });

  vkDestroyDescriptorSetLayout(context.device, ds_layout, nullptr);
  DestroyPipeline(context, compute_pipeline);
}

void CreateEnvMap(VulkanContext &context, ImmediateSubmit &immediate_submit,
                  DescriptorBuilder &descriptor_builder, VkSampler sampler,
                  AllocatedImage equirect_map, Skybox &skybox) {
  // Must also change resolution in comp shader
  VkExtent3D cube_map_size = {512, 512, 1};

  CreateAllocatedImage(context, cube_map_size, VK_FORMAT_R16G16B16A16_SFLOAT,
                       VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
                       skybox.image, 1, true);

  Pipeline compute_pipeline;
  VkDescriptorSetLayout ds_layout;
  VkDescriptorSet ds;

  descriptor_builder.Reset();
  descriptor_builder.BindCombinedImage(0, equirect_map.image_view, sampler);
  descriptor_builder.BindStorageImage(1, skybox.image.image_view);
  descriptor_builder.Build(context, VK_SHADER_STAGE_COMPUTE_BIT, ds, ds_layout);

  ComputePipelineBuilder pipeline_builder{};
  pipeline_builder.SetShader(context, "equirectangular_to_envmap.comp.spv");
  pipeline_builder.AddDescriptorSetLayout(ds_layout);
  pipeline_builder.Build(context, compute_pipeline);

  immediate_submit.Submit(context, [&](VkCommandBuffer cmd) {
    TransitionImage(cmd, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
                    skybox.image.image);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                      compute_pipeline.obj);

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                            compute_pipeline.layout, 0, 1, &ds, 0, nullptr);

    vkCmdDispatch(cmd, (cube_map_size.width + 15) / 16,
                  (cube_map_size.width + 15) / 16, 6);

    TransitionImage(cmd, VK_IMAGE_LAYOUT_GENERAL,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    skybox.image.image);
  });

  vkDestroyDescriptorSetLayout(context.device, ds_layout, nullptr);
  DestroyPipeline(context, compute_pipeline);
}

void CreateIrradiance(VulkanContext &context, ImmediateSubmit &immediate_submit,
                      DescriptorBuilder &descriptor_builder, VkSampler sampler,
                      Skybox &skybox) {
  // Change resolution / sample count in compute
  VkExtent3D cube_map_size = {32, 32, 1};

  CreateAllocatedImage(context, cube_map_size, VK_FORMAT_R16G16B16A16_SFLOAT,
                       VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
                       skybox.irradiance, 1, true);

  Pipeline compute_pipeline;
  VkDescriptorSetLayout ds_layout;
  VkDescriptorSet ds;

  descriptor_builder.Reset();
  descriptor_builder.BindCombinedImage(0, skybox.image.image_view, sampler);
  descriptor_builder.BindStorageImage(1, skybox.irradiance.image_view);
  descriptor_builder.Build(context, VK_SHADER_STAGE_COMPUTE_BIT, ds, ds_layout);

  ComputePipelineBuilder pipeline_builder{};
  pipeline_builder.SetShader(context, "irradiance_convolution.comp.spv");
  pipeline_builder.AddDescriptorSetLayout(ds_layout);
  pipeline_builder.Build(context, compute_pipeline);

  immediate_submit.Submit(context, [&](VkCommandBuffer cmd) {
    TransitionImage(cmd, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
                    skybox.irradiance.image);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                      compute_pipeline.obj);

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                            compute_pipeline.layout, 0, 1, &ds, 0, nullptr);

    vkCmdDispatch(cmd, (cube_map_size.width + 7) / 8,
                  (cube_map_size.height + 7) / 8, 6);

    TransitionImage(cmd, VK_IMAGE_LAYOUT_GENERAL,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    skybox.irradiance.image);
  });

  vkDestroyDescriptorSetLayout(context.device, ds_layout, nullptr);
  DestroyPipeline(context, compute_pipeline);
}

void CreatePrefilter(VulkanContext &context, ImmediateSubmit &immediate_submit,
                     DescriptorBuilder &descriptor_builder, VkSampler sampler,
                     Skybox &skybox) {
  const uint32_t MIP_LEVELS = 5;

  // no need the change in comp shader too
  const VkExtent3D cube_map_size = {128, 128, 1};
  CreateAllocatedImage(context, cube_map_size, VK_FORMAT_R16G16B16A16_SFLOAT,
                       VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
                           VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                           VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                       skybox.prefilter, MIP_LEVELS, true);

  GenerateMipmaps(context, MIP_LEVELS, skybox.prefilter);

  Pipeline compute_pipeline;
  VkDescriptorSetLayout ds_layout;
  VkDescriptorSet ds;

  struct PushConstants {
    uint32_t resolution;
    uint32_t mip_level;
    uint32_t sample_count;
  };

  std::vector<VkImageView> mip_image_views;
  mip_image_views.resize(MIP_LEVELS);

  for (uint32_t i = 0; i < MIP_LEVELS; i++) {
    VkImageViewCreateInfo image_view_ci = vkinit::ImageViewCI(
        VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT,
        skybox.prefilter.image, 1);
    image_view_ci.subresourceRange.baseMipLevel = i;
    image_view_ci.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
    image_view_ci.subresourceRange.layerCount = 6;

    VK_CHECK(vkCreateImageView(context.device, &image_view_ci, nullptr,
                               &mip_image_views[i]));
  }

  descriptor_builder.Reset();
  descriptor_builder.BindCombinedImage(0, skybox.image.image_view, sampler);
  descriptor_builder.BindStorageImages(1, mip_image_views);
  descriptor_builder.Build(context, VK_SHADER_STAGE_COMPUTE_BIT, ds, ds_layout);

  ComputePipelineBuilder pipeline_builder{};
  pipeline_builder.SetShader(context, "prefilter.comp.spv");
  pipeline_builder.AddDescriptorSetLayout(ds_layout);
  pipeline_builder.AddPushConstantRange(sizeof(PushConstants));
  pipeline_builder.Build(context, compute_pipeline);

  immediate_submit.Submit(context, [&](VkCommandBuffer cmd) {
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                      compute_pipeline.obj);

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                            compute_pipeline.layout, 0, 1, &ds, 0, nullptr);

    TransitionImage(cmd, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
                    skybox.prefilter.image, MIP_LEVELS);

    for (uint32_t mip = 0; mip < MIP_LEVELS; mip++) {
      PushConstants pc{
          .resolution = cube_map_size.width,
          .mip_level = mip,
          .sample_count = 128,
      };

      uint32_t size = cube_map_size.width >> mip;

      vkCmdPushConstants(cmd, compute_pipeline.layout,
                         VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstants),
                         &pc);

      uint32_t group_count = (size + 7) / 8;

      vkCmdDispatch(cmd, group_count, group_count, 6);
    }

    TransitionImage(cmd, VK_IMAGE_LAYOUT_GENERAL,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    skybox.prefilter.image, MIP_LEVELS);
  });

  for (uint32_t i = 0; i < MIP_LEVELS; i++) {
    vkDestroyImageView(context.device, mip_image_views[i], nullptr);
  }

  vkDestroyDescriptorSetLayout(context.device, ds_layout, nullptr);
  DestroyPipeline(context, compute_pipeline);
}

void CreateSkybox(VulkanContext &context, ImmediateSubmit &immediate_submit,
                  DescriptorBuilder &descriptor_builder,
                  TextureManager &texture_manager, std::string file_name,
                  Skybox &skybox) {

  std::string full_path = (file_name + ".hdr");

  AllocatedImage skybox_image =
      texture_manager.texture_data[texture_manager.texture_indices[full_path]];

  VkSampler sampler;
  CreateImageSampler(context, sampler);

  CreateEnvMap(context, immediate_submit, descriptor_builder, sampler,
               skybox_image, skybox);
  CreateIrradiance(context, immediate_submit, descriptor_builder, sampler,
                   skybox);
  CreatePrefilter(context, immediate_submit, descriptor_builder, sampler,
                  skybox);
  GenerateBrdfLut(context, immediate_submit, descriptor_builder, sampler,
                  skybox);

  DestroyImageSampler(context, sampler);
}

void DestroySkybox(VulkanContext &context, Skybox &skybox) {
  DestroyAllocatedImage(context, skybox.image);
  DestroyAllocatedImage(context, skybox.brdf);
  DestroyAllocatedImage(context, skybox.irradiance);
  DestroyAllocatedImage(context, skybox.prefilter);
}
