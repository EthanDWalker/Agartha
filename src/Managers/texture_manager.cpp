#include "texture_manager.h"
#include "Backend/allocated_image.h"
#include "Backend/context.h"
#include "Backend/descriptors.h"
#include "Backend/pipeline.h"
#include "Loaders/image.h"
#include "Loaders/model.h"
#include <cmath>
#include <future>
#include <mutex>
#include <vector>

void TextureManager::Init(VulkanContext &context,
                          DescriptorBuilder &descriptor_builder) {
  texture_data.resize(MAX_TEXTURES);

  CreateImageSampler(context, sampler);

  uint32_t alloc_scaler = descriptor_builder.pool.alloc_scaler;
  descriptor_builder.pool.alloc_scaler = std::ceil(MAX_TEXTURES / 3.0f);
  descriptor_builder.Reset();
  descriptor_builder.BindImages(0, texture_data);
  descriptor_builder.BindSampler(1, sampler);
  descriptor_builder.Build(context,
                           VK_SHADER_STAGE_FRAGMENT_BIT |
                               VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
                           descriptor_set, descriptor_set_layout);
  descriptor_builder.pool.alloc_scaler = alloc_scaler;
}

void TextureManager::LoadTexture(VulkanContext &context, std::string filename,
                                 AllocatedImage &image) {
  VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;
  bool float_load = false;

  if (filename.ends_with(".hdr")) {
    format = VK_FORMAT_R32G32B32A32_SFLOAT;
    float_load = true;
  }

  ImageData image_data;
  LoadImageData(filename, image_data, float_load);

  VkExtent3D image_extent = {
      static_cast<uint32_t>(image_data.width),
      static_cast<uint32_t>(image_data.height),
      1,
  };

  const uint8_t channel_count = 4; // @HARDCODE forced in stbi_image_load
  CreateImageDataAsync(
      context, image_data.data, channel_count, image_extent, format,
      VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT |
          VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
      image);

  DestroyImageData(image_data);
}

uint32_t TextureManager::UploadTexture(VulkanContext &context,
                                       std::string filename) {
  uint32_t index = 0;
  {
    std::lock_guard<std::mutex> lock(texture_mutex);
    if (texture_indices.find(filename) != texture_indices.end()) {
      return texture_indices[filename];
    }
    index = texture_index;
    texture_indices[filename] = texture_index;
  }

  std::thread([=, this, &context]() {
    AllocatedImage image{};
    LoadTexture(context, filename, image);

    {
      std::lock_guard<std::mutex> lock(texture_mutex);
      texture_data[index] = image;
    }

    VkDescriptorImageInfo image_info{};
    image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    image_info.imageView = image.image_view;

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    write.pImageInfo = &image_info;
    write.dstBinding = 0;
    write.dstArrayElement = index;
    write.dstSet = descriptor_set;

    vkUpdateDescriptorSets(context.device, 1, &write, 0, nullptr);
  }).detach();

  return texture_index++;
};

uint32_t TextureManager::AddAllocatedImage(VulkanContext &context,
                                           AllocatedImage &image) {
  uint32_t index = 0;
  {
    std::lock_guard<std::mutex> lock(texture_mutex);
    index = texture_index;
    texture_data[index] = image;
  }

  VkDescriptorImageInfo image_info{};
  image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  image_info.imageView = image.image_view;

  VkWriteDescriptorSet write{};
  write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  write.descriptorCount = 1;
  write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
  write.pImageInfo = &image_info;
  write.dstBinding = 0;
  write.dstArrayElement = index;
  write.dstSet = descriptor_set;

  vkUpdateDescriptorSets(context.device, 1, &write, 0, nullptr);

  return texture_index++;
}

Material TextureManager::UploadMaterial(VulkanContext &context,
                                        DescriptorBuilder &descriptor_builder,
                                        MaterialData data) {
  Material material;

  VkDescriptorSet input_descriptor_set;
  VkDescriptorSetLayout input_descriptor_layout;
  VkDescriptorSet output_descriptor_set;
  VkDescriptorSetLayout output_descriptor_layout;

  AllocatedImage albedo{};
  AllocatedImage ao{};
  AllocatedImage mr{};
  AllocatedImage normal{};

  std::vector<std::future<void>> futures{};
  futures.push_back(std::async(std::launch::async, [&]() {
    if (data.albedo.empty())
      return;
    LoadTexture(context, data.albedo, albedo);
  }));
  futures.push_back(std::async(std::launch::async, [&]() {
    if (data.ambient_occlusion.empty())
      return;
    LoadTexture(context, data.ambient_occlusion, ao);
  }));
  futures.push_back(std::async(std::launch::async, [&]() {
    if (data.metal_roughness.empty())
      return;
    LoadTexture(context, data.metal_roughness, mr);
  }));
  futures.push_back(std::async(std::launch::async, [&]() {
    if (data.normal.empty())
      return;
    LoadTexture(context, data.normal, normal);
  }));

  for (auto &future : futures) {
    future.get();
  }

  descriptor_builder.Reset();
  descriptor_builder.BindStorageImage(0, albedo.image_view);
  descriptor_builder.BindStorageImage(1, mr.image_view);
  descriptor_builder.BindStorageImage(2, normal.image_view);
  descriptor_builder.BindStorageImage(3, ao.image_view);
  descriptor_builder.Build(context, VK_SHADER_STAGE_COMPUTE_BIT,
                           input_descriptor_set, input_descriptor_layout);

  AllocatedImage albedo_ao{};
  AllocatedImage mr_normal{};

  CreateAllocatedImage(context, albedo.extent, albedo.format,
                       VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                       albedo_ao, true);
  CreateAllocatedImage(context, albedo.extent, albedo.format,
                       VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                       mr_normal, true);

  descriptor_builder.Reset();
  descriptor_builder.BindStorageImage(0, albedo_ao.image_view);
  descriptor_builder.BindStorageImage(1, mr_normal.image_view);
  descriptor_builder.Build(context, VK_SHADER_STAGE_COMPUTE_BIT,
                           output_descriptor_set, output_descriptor_layout);

  material.albedo_ao = AddAllocatedImage(context, albedo_ao);
  material.mr_normal = AddAllocatedImage(context, mr_normal);

  std::thread([=, this, &context]() {
    Pipeline pipeline{};
    ComputePipelineBuilder pipeline_builder{};
    pipeline_builder.SetShader(context, "pack_material.comp.spv");
    pipeline_builder.AddDescriptorSetLayout(input_descriptor_layout);
    pipeline_builder.AddDescriptorSetLayout(output_descriptor_layout);
    pipeline_builder.Build(context, pipeline);

    ImmediateSubmit::SubmitAsync(context, [&](VkCommandBuffer cmd) {
      TransitionImage(cmd, {}, VK_ACCESS_2_SHADER_WRITE_BIT, {},
                      VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                      VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
                      albedo_ao.image);
      TransitionImage(cmd, {}, VK_ACCESS_2_SHADER_WRITE_BIT, {},
                      VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                      VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
                      mr_normal.image);

      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline.obj);

      std::array<VkDescriptorSet, 2> ds = {
          input_descriptor_set,
          output_descriptor_set,
      };

      vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                              pipeline.layout, 0, ds.size(), ds.data(), 0,
                              nullptr);

      vkCmdDispatch(cmd, std::ceil(albedo.extent.width / 16.0f),
                    std::ceil(albedo.extent.height / 16.0f), 1);

      AllocatedImage albedo_ao;
      AllocatedImage mr_normal;
      {
        std::lock_guard<std::mutex> lock(texture_mutex);
        albedo_ao = texture_data[material.albedo_ao];
        mr_normal = texture_data[material.mr_normal];
      }
      GenerateMipmaps(cmd, albedo_ao);
      GenerateMipmaps(cmd, mr_normal);
    });

    vmaDestroyImage(context.allocator, albedo.image, albedo.allocation);
    vkDestroyImageView(context.device, albedo.image_view, nullptr);

    vmaDestroyImage(context.allocator, ao.image, ao.allocation);
    vkDestroyImageView(context.device, ao.image_view, nullptr);

    vmaDestroyImage(context.allocator, normal.image, normal.allocation);
    vkDestroyImageView(context.device, normal.image_view, nullptr);

    vmaDestroyImage(context.allocator, mr.image, mr.allocation);
    vkDestroyImageView(context.device, mr.image_view, nullptr);

    DestroyPipeline(context, pipeline);
    vkDestroyDescriptorSetLayout(context.device, input_descriptor_layout,
                                 nullptr);
    vkDestroyDescriptorSetLayout(context.device, output_descriptor_layout,
                                 nullptr);
  }).detach();

  return material;
}

void TextureManager::Destroy(VulkanContext &context) {
  for (auto &image : texture_data) {
    if (image.extent.depth != 0) {
      DestroyAllocatedImage(context, image);
    }
  }

  DestroyImageSampler(context, sampler);

  vkDestroyDescriptorSetLayout(context.device, descriptor_set_layout, nullptr);
}
