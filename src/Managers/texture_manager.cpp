#include "texture_manager.h"
#include "Backend/allocated_image.h"
#include "Backend/buffer.h"
#include "Backend/context.h"
#include "Backend/descriptors.h"
#include "Backend/image_format.h"
#include "Backend/immediate_submit.h"
#include "Loaders/image.h"
#include "Loaders/model.h"
#include <cmath>
#include <mutex>
#include <stdlib.h>

void TextureManager::Init(VulkanContext &context,
                          DescriptorBuilder &descriptor_builder) {
  texture_data.resize(MAX_TEXTURES);

  CreateImageSampler(context, sampler);

  uint32_t alloc_scaler = descriptor_builder.pool.alloc_scaler;
  descriptor_builder.pool.alloc_scaler = std::ceil(MAX_TEXTURES / 3.0f);
  descriptor_builder.Reset();
  descriptor_builder.BindImages(0, texture_data);
  descriptor_builder.BindSampler(1, sampler);
  descriptor_builder.Build(context, VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
                           descriptor_set, descriptor_set_layout);
  descriptor_builder.pool.alloc_scaler = alloc_scaler;
}

uint32_t TextureManager::GetTexture(VulkanContext &context,
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

    AllocatedImage image{};
    CreateAllocatedImage(context, image_extent, format,
                         VK_IMAGE_USAGE_SAMPLED_BIT |
                             VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                             VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                         image, true);

    {
      std::lock_guard<std::mutex> lock(texture_mutex);
      texture_data[index] = image;
    }

    const uint8_t channel_count = 4; // @HARDCODE forced in stbi_image_load
    size_t data_size = image_extent.depth * image_extent.width *
                       image_extent.height * channel_count *
                       GetFormatComponentSize(format);

    AllocatedBuffer upload_buffer;
    CreateBuffer(context, data_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VMA_MEMORY_USAGE_CPU_TO_GPU, upload_buffer);

    memcpy(upload_buffer.info.pMappedData, image_data.data, data_size);

    ImmediateSubmit::SubmitAsync(context, [&](VkCommandBuffer cmd) {
      TransitionImage(cmd, VK_IMAGE_LAYOUT_UNDEFINED,
                      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, image.image);
      VkBufferImageCopy copy_region{};
      copy_region.bufferOffset = 0;
      copy_region.bufferRowLength = 0;
      copy_region.bufferImageHeight = 0;
      copy_region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      copy_region.imageSubresource.mipLevel = 0;
      copy_region.imageSubresource.baseArrayLayer = 0;
      copy_region.imageSubresource.layerCount = 1;
      copy_region.imageExtent = image_extent;

      vkCmdCopyBufferToImage(cmd, upload_buffer.buffer, image.image,
                             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                             &copy_region);

      TransitionImage(cmd, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, image.image);
      GenerateMipmaps(cmd, image);
    });

    DestroyBuffer(context, upload_buffer);

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

    DestroyImageData(image_data);
  }).detach();

  return texture_index++;
};

Material TextureManager::GetMaterial(VulkanContext &context,
                                     MaterialData data) {
  Material material;
  material.albedo =
      !data.albedo.empty() ? GetTexture(context, data.albedo) : -1;
  material.normal =
      !data.normal.empty() ? GetTexture(context, data.normal) : -1;

  material.emissive =
      !data.emissive.empty() ? GetTexture(context, data.emissive) : -1;

  material.ambient_occlusion = !data.ambient_occlusion.empty()
                                   ? GetTexture(context, data.ambient_occlusion)
                                   : -1;
  material.metal_roughness = !data.metal_roughness.empty()
                                 ? GetTexture(context, data.metal_roughness)
                                 : -1;
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
