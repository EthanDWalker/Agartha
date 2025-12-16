#include "texture_manager.h"
#include "Backend/allocated_image.h"
#include "Backend/context.h"
#include "Backend/descriptors.h"
#include "Backend/immediate_submit.h"
#include "Backend/pipeline.h"
#include "Parsers/image.h"
#include "Parsers/model.h"
#include "fmt/base.h"
#include <cmath>
#include <future>
#include <mutex>
#include <vector>

void TextureManager::Init(DescriptorBuilder &descriptor_builder) {
  texture_data.resize(MAX_TEXTURES);

  CreateImageSampler(sampler);

  uint32_t alloc_scaler = descriptor_builder.pool.alloc_scaler;
  descriptor_builder.pool.alloc_scaler = std::ceil(MAX_TEXTURES / 3.0f);
  descriptor_builder.BindImages(0, texture_data);
  descriptor_builder.BindSampler(1, sampler);
  descriptor_builder.Build(VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
                           descriptor_set, descriptor_set_layout);
  descriptor_builder.pool.alloc_scaler = alloc_scaler;

  AllocatedImage place_holder = texture_data.front();
  descriptor_builder.BindStorageImage(0, place_holder.image_view);
  descriptor_builder.BindStorageImage(1, place_holder.image_view);
  descriptor_builder.BindStorageImage(2, place_holder.image_view);
  descriptor_builder.BindStorageImage(3, place_holder.image_view);
  descriptor_builder.Build(VK_SHADER_STAGE_COMPUTE_BIT, pack_input_descriptor_set,
                           pack_input_descriptor_layout);

  descriptor_builder.BindStorageImage(0, place_holder.image_view);
  descriptor_builder.BindStorageImage(1, place_holder.image_view);
  descriptor_builder.Build(VK_SHADER_STAGE_COMPUTE_BIT, pack_output_descriptor_set,
                           pack_output_descriptor_layout);

  {
    ComputePipelineBuilder pipeline_builder{};
    pipeline_builder.SetShader("pack_material.comp.spv");
    pipeline_builder.AddDescriptorSetLayout(pack_input_descriptor_layout);
    pipeline_builder.AddDescriptorSetLayout(pack_output_descriptor_layout);
    pipeline_builder.Build(pack_pipeline);
  }

  UploadTexture(DEFAULT_ALBEDO_MAP);
  UploadTexture(DEFAULT_METALLIC_ROUGHNESS_MAP);
  UploadTexture(DEFAULT_NORMAL_MAP);
}

void TextureManager::LoadTexture(std::string filename, AllocatedImage &image,
                                 glm::ivec2 forced_extent) {
  VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;
  bool float_load = false;

  if (filename.ends_with(".hdr")) {
    format = VK_FORMAT_R32G32B32A32_SFLOAT;
    float_load = true;
  }

  ImageData image_data;
  ParseImageData(filename, image_data, float_load);
  if (forced_extent != glm::ivec2(0)) {
    ResizeImageData(image_data, forced_extent);
  }
  VkExtent3D image_extent = {
      static_cast<uint32_t>(image_data.width),
      static_cast<uint32_t>(image_data.height),
      1,
  };

  const uint8_t channel_count = 4; // @HARDCODE forced in stbi_image_load
  CreateImageDataAsync(image_data.data, channel_count, image_extent, format,
                       VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT |
                           VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                       image);

  DestroyImageData(image_data);
}

uint32_t TextureManager::UploadTexture(std::string filename) {
  uint32_t index = 0;
  {
    std::lock_guard<std::mutex> lock(texture_mutex);
    if (texture_indices.find(filename) != texture_indices.end()) {
      return texture_indices[filename];
    }
    index = texture_index;
    texture_indices[filename] = texture_index;
  }

  std::thread([=, this]() {
    AllocatedImage image{};
    LoadTexture(filename, image);

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

    vkUpdateDescriptorSets(VulkanContext::device, 1, &write, 0, nullptr);
  }).detach();

  texture_index++;
  return index;
};

uint32_t TextureManager::AddAllocatedImage(AllocatedImage &image) {
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

  vkUpdateDescriptorSets(VulkanContext::device, 1, &write, 0, nullptr);

  return texture_index++;
}

Material TextureManager::UploadMaterial(MaterialData &data) {
  Material material;

  AllocatedImage albedo{};
  AllocatedImage ao{};
  AllocatedImage mr{};
  AllocatedImage normal{};

  AllocatedImage albedo_ao{};
  AllocatedImage mr_normal{};

  {
    std::lock_guard<std::mutex> lock(texture_mutex);
    if (texture_indices.find(data.albedo) != texture_indices.end() &&
        texture_indices.find(data.normal) != texture_indices.end() && !data.albedo.empty() &&
        !data.normal.empty()) {
      material.albedo_ao = texture_indices[data.albedo];
      material.mr_normal = texture_indices[data.normal];
      return material;
    }
  }

  std::vector<std::future<void>> futures{};
  futures.push_back(std::async(std::launch::async, [&]() {
    if (data.albedo.empty()) {
      albedo = texture_data[texture_indices[DEFAULT_ALBEDO_MAP]];
      return;
    }
    LoadTexture(data.albedo, albedo);
  }));
  futures.push_back(std::async(std::launch::async, [&]() {
    if (data.ambient_occlusion.empty())
      return;
    LoadTexture(data.ambient_occlusion, ao);
  }));
  futures.push_back(std::async(std::launch::async, [&]() {
    if (data.metal_roughness.empty()) {
      mr = texture_data[texture_indices[DEFAULT_METALLIC_ROUGHNESS_MAP]];
      return;
    }
    LoadTexture(data.metal_roughness, mr);
  }));
  futures.push_back(std::async(std::launch::async, [&]() {
    if (data.normal.empty()) {
      normal = texture_data[texture_indices[DEFAULT_NORMAL_MAP]];
      return;
    }
    LoadTexture(data.normal, normal);
  }));

  for (auto &future : futures) {
    future.get();
  }

  UpdateDescriptorSetStorageImage(albedo, pack_input_descriptor_set, 0);
  UpdateDescriptorSetStorageImage(mr, pack_input_descriptor_set, 1);
  UpdateDescriptorSetStorageImage(normal, pack_input_descriptor_set, 2);
  UpdateDescriptorSetStorageImage(ao, pack_input_descriptor_set, 3);

  CreateAllocatedImage(albedo.extent, albedo.format,
                       VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                       albedo_ao, true);
  CreateAllocatedImage(albedo.extent, albedo.format,
                       VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                       mr_normal, true);

  UpdateDescriptorSetStorageImage(albedo_ao, pack_output_descriptor_set, 0);
  UpdateDescriptorSetStorageImage(mr_normal, pack_output_descriptor_set, 1);

  material.albedo_ao = AddAllocatedImage(albedo_ao);
  material.mr_normal = AddAllocatedImage(mr_normal);

  texture_indices[data.albedo] = material.albedo_ao;
  texture_indices[data.normal] = material.mr_normal;

  std::thread([=, this]() {
    ImmediateSubmit::Submit([&](VkCommandBuffer cmd) {
      TransitionImage(cmd, {}, VK_ACCESS_2_SHADER_WRITE_BIT, {},
                      VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                      VK_IMAGE_LAYOUT_GENERAL, albedo_ao.image);
      TransitionImage(cmd, {}, VK_ACCESS_2_SHADER_WRITE_BIT, {},
                      VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                      VK_IMAGE_LAYOUT_GENERAL, mr_normal.image);

      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pack_pipeline.obj);

      std::array<VkDescriptorSet, 2> ds = {
          pack_input_descriptor_set,
          pack_output_descriptor_set,
      };

      vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pack_pipeline.layout, 0,
                              ds.size(), ds.data(), 0, nullptr);

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

    if (!data.albedo.empty()) {
      vmaDestroyImage(VulkanContext::allocator, albedo.image, albedo.allocation);
      vkDestroyImageView(VulkanContext::device, albedo.image_view, nullptr);
    }

    if (!data.ambient_occlusion.empty()) {
      vmaDestroyImage(VulkanContext::allocator, ao.image, ao.allocation);
      vkDestroyImageView(VulkanContext::device, ao.image_view, nullptr);
    }

    if (!data.normal.empty()) {
      vmaDestroyImage(VulkanContext::allocator, normal.image, normal.allocation);
      vkDestroyImageView(VulkanContext::device, normal.image_view, nullptr);
    }

    if (!data.metal_roughness.empty()) {
      vmaDestroyImage(VulkanContext::allocator, mr.image, mr.allocation);
      vkDestroyImageView(VulkanContext::device, mr.image_view, nullptr);
    }
  }).detach();

  return material;
}

void TextureManager::Destroy() {
  for (auto &image : texture_data) {
    if (image.extent.depth != 0) {
      DestroyAllocatedImage(image);
    }
  }

  DestroyPipeline(pack_pipeline);
  vkDestroyDescriptorSetLayout(VulkanContext::device, pack_input_descriptor_layout, nullptr);
  vkDestroyDescriptorSetLayout(VulkanContext::device, pack_output_descriptor_layout, nullptr);

  DestroyImageSampler(sampler);

  vkDestroyDescriptorSetLayout(VulkanContext::device, descriptor_set_layout, nullptr);
}
