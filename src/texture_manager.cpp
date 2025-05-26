#include "texture_manager.h"
#include "Backend/allocated_image.h"
#include "Backend/buffer.h"
#include "Backend/context.h"
#include "Backend/descriptors.h"
#include "Backend/image_format.h"
#include "Backend/immediate_submit.h"
#include "Loaders/image.h"
#include "Loaders/model.h"
#include "fmt/base.h"
#include "timer.h"
#include <cmath>
#include <filesystem>
#include <future>
#include <mutex>
#include <stdlib.h>

void TextureManager::Init(VulkanContext &context,
                          DescriptorBuilder &descriptor_builder) {
  Timer timer{};
  texture_data.resize(MAX_TEXTURES);
  uint32_t alloc_scaler = descriptor_builder.pool.alloc_scaler;
  descriptor_builder.pool.alloc_scaler = std::ceil(MAX_TEXTURES / 3.0f);
  descriptor_builder.Reset();
  descriptor_builder.BindImages(0, texture_data);
  descriptor_builder.Build(context, VK_SHADER_STAGE_FRAGMENT_BIT,
                           descriptor_set, descriptor_set_layout);
  descriptor_builder.pool.alloc_scaler = alloc_scaler;

  struct LoadedImageTask {
    ImageData image_data;
    AllocatedImage texture_image;
    std::string file_name;
  };

  std::vector<std::future<LoadedImageTask>> futures;

  std::mutex queue_mutex;

  for (const auto &file : std::filesystem::directory_iterator(texture_dir)) {
    if (file.is_directory()) {
      continue;
    }
    std::string file_name = file.path().filename().string();

    futures.push_back(std::async(std::launch::async, [file_name, &context,
                                                      &queue_mutex]() {
      LoadedImageTask task;
      task.file_name = file_name;

      VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;
      bool float_load = false;
      if (file_name.ends_with(".hdr")) {
        format = VK_FORMAT_R32G32B32A32_SFLOAT;
        float_load = true;
      }
      LoadImageData(file_name, task.image_data, float_load);

      VkExtent3D image_extent = {
          static_cast<uint32_t>(task.image_data.width),
          static_cast<uint32_t>(task.image_data.height),
          1,
      };

      const uint8_t channel_count = 4; // @HARDCODE forced in stbi_image_load
      size_t data_size = image_extent.depth * image_extent.width *
                         image_extent.height * channel_count *
                         GetFormatComponentSize(format);

      AllocatedBuffer upload_buffer;
      CreateBuffer(context, data_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                   VMA_MEMORY_USAGE_CPU_TO_GPU, upload_buffer);

      memcpy(upload_buffer.info.pMappedData, task.image_data.data, data_size);

      CreateAllocatedImage(context, image_extent, format,
                           VK_IMAGE_USAGE_SAMPLED_BIT |
                               VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                               VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                           task.texture_image);

      std::lock_guard<std::mutex> lock(queue_mutex);

      ImmediateSubmit::SubmitAsync(context, [&](VkCommandBuffer cmd) {
        TransitionImage(cmd, VK_IMAGE_LAYOUT_UNDEFINED,
                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        task.texture_image.image);
        VkBufferImageCopy copy_region{};
        copy_region.bufferOffset = 0;
        copy_region.bufferRowLength = 0;
        copy_region.bufferImageHeight = 0;
        copy_region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copy_region.imageSubresource.mipLevel = 0;
        copy_region.imageSubresource.baseArrayLayer = 0;
        copy_region.imageSubresource.layerCount = 1;
        copy_region.imageExtent = image_extent;

        vkCmdCopyBufferToImage(
            cmd, upload_buffer.buffer, task.texture_image.image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy_region);

        TransitionImage(cmd, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                        task.texture_image.image);
      });

      DestroyBuffer(context, upload_buffer);

      return task;
    }));
  }

  uint32_t file_index = 0;
  std::vector<VkDescriptorImageInfo> image_infos{};

  for (auto &future : futures) {
    LoadedImageTask image_task = future.get();

    texture_data[file_index] = image_task.texture_image;

    texture_indices[image_task.file_name] = file_index;

    VkDescriptorImageInfo image_info{};
    image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    image_info.imageView = image_task.texture_image.image_view;
    image_infos.push_back(image_info);

    file_index++;
    if (file_index > MAX_TEXTURES) {
      fmt::println("[TEXTURE_MANAGER] Textures > MAX_TEXTURES: {}",
                   MAX_TEXTURES);
      abort();
    }
  }

  VkWriteDescriptorSet write{};
  write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  write.descriptorCount = image_infos.size();
  write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
  write.pImageInfo = image_infos.data();
  write.dstBinding = 0;
  write.dstArrayElement = 0;
  write.dstSet = descriptor_set;

  vkUpdateDescriptorSets(context.device, 1, &write, 0, nullptr);

  fmt::println("TextureManager::Init: {}", timer.ElapsedMillis());
}

Material TextureManager::GetMaterial(MaterialData data) {
  Material material;
  material.albedo = !data.albedo.empty() ? texture_indices[data.albedo] : -1;
  material.normal = !data.normal.empty() ? texture_indices[data.normal] : -1;

  material.emissive =
      !data.emissive.empty() ? texture_indices[data.emissive] : -1;

  material.ambient_occlusion = !data.ambient_occlusion.empty()
                                   ? texture_indices[data.ambient_occlusion]
                                   : -1;
  material.metal_roughness = !data.metal_roughness.empty()
                                 ? texture_indices[data.metal_roughness]
                                 : -1;
  return material;
}

void TextureManager::Destroy(VulkanContext &context) {
  for (auto &image : texture_data) {
    if (image.extent.depth != 0) {
      DestroyAllocatedImage(context, image);
    }
  }
  vkDestroyDescriptorSetLayout(context.device, descriptor_set_layout, nullptr);
}
