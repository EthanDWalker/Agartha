#include "skybox.h"

#include "Backend/allocated_image.h"
#include "Backend/context.h"
#include "Backend/descriptors.h"
#include "Backend/immediate_submit.h"
#include "Backend/pipeline.h"
#include "Loaders/image.h"
#include <cstdint>
#include <vulkan/vulkan.h>

void CreateIrradiance(VulkanContext &context, ImmediateSubmit &immediate_submit,
                      DesciptorBuilder &descriptor_builder, VkSampler sampler,
                      Skybox &skybox) {
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
  pipeline_builder.SetShader(context, "irradience_convolution.comp.spv");
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
                     DesciptorBuilder &descriptor_builder, VkSampler sampler,
                     Skybox &skybox) {
  const uint32_t MIP_LEVELS = 5;

  VkExtent3D cube_map_size = {128, 128, 1};
  CreateAllocatedImage(context, cube_map_size, VK_FORMAT_R16G16B16A16_SFLOAT,
                       VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
                           VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                           VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                       skybox.prefilter, MIP_LEVELS, true);

  GenerateMipmaps(context, immediate_submit, MIP_LEVELS, skybox.prefilter);

  Pipeline compute_pipeline;
  VkDescriptorSetLayout ds_layout;
  VkDescriptorSet ds;

  struct PushConstants {
    float roughness;
    uint32_t mip_level;
    uint32_t resolution;
    uint32_t sample_count;
  };

  descriptor_builder.Reset();
  descriptor_builder.BindCombinedImage(0, skybox.irradiance.image_view,
                                       sampler);
  descriptor_builder.BindStorageImage(1, skybox.prefilter.image_view);
  descriptor_builder.Build(context, VK_SHADER_STAGE_COMPUTE_BIT, ds, ds_layout);

  ComputePipelineBuilder pipeline_builder{};
  pipeline_builder.SetShader(context, "prefilter_envmap.comp.spv");
  pipeline_builder.AddDescriptorSetLayout(ds_layout);
  pipeline_builder.AddPushConstantRange(sizeof(PushConstants));
  pipeline_builder.Build(context, compute_pipeline);

  immediate_submit.Submit(context, [&](VkCommandBuffer cmd) {
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                            compute_pipeline.layout, 0, 1, &ds, 0, nullptr);

    for (uint32_t mip = 0; mip < MIP_LEVELS; mip++) {
      uint32_t mip_resolution = 128 >> mip;
      float roughness =
          static_cast<float>(mip) / static_cast<float>(MIP_LEVELS - 1);

      PushConstants pc{
          .roughness = roughness,
          .mip_level = mip,
          .resolution = mip_resolution,
          .sample_count = 1024,
      };

      vkCmdPushConstants(cmd, compute_pipeline.layout,
                         VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstants),
                         &pc);

      uint32_t group_count = (mip_resolution + 7) / 8;

      vkCmdDispatch(cmd, group_count, group_count, 6);
    }
  });

  vkDestroyDescriptorSetLayout(context.device, ds_layout, nullptr);
  DestroyPipeline(context, compute_pipeline);
}

void CreateSkybox(VulkanContext &context, ImmediateSubmit &immediate_submit,
                  DesciptorBuilder &descriptor_builder, std::string file_name,
                  Skybox &skybox) {
  std::string full_path = (file_name + ".hdr");

  ImageData image_data;
  LoadImageData(full_path, image_data, true, true);

  VkExtent3D image_size = {static_cast<uint32_t>(image_data.width),
                           static_cast<uint32_t>(image_data.height), 1};

  CreateAllocatedImageData(context, immediate_submit, image_data.data,
                           image_size, VK_FORMAT_R32G32B32A32_SFLOAT,
                           VK_IMAGE_USAGE_SAMPLED_BIT, skybox.image);

  DestroyImageData(image_data);

  VkSampler sampler;
  CreateImageSampler(context, sampler);

  CreateIrradiance(context, immediate_submit, descriptor_builder, sampler,
                   skybox);
  /*
   * WORK IN PROGRESS
  CreatePrefilter(context, immediate_submit, descriptor_builder, sampler,
                  skybox);
  */

  DestroyImageSampler(context, sampler);
}

void DestroySkybox(VulkanContext &context, Skybox &skybox) {
  DestroyAllocatedImage(context, skybox.image);
  DestroyAllocatedImage(context, skybox.brdf);
  DestroyAllocatedImage(context, skybox.irradiance);
  DestroyAllocatedImage(context, skybox.prefilter);
}
