#include "texture.h"
#include "Backend/allocated_image.h"
#include "Backend/context.h"
#include "Backend/descriptors.h"
#include "Backend/immediate_submit.h"
#include "Backend/pipeline.h"
#include "Loaders/image.h"
#include <array>
#include <cstdint>
#include <vulkan/vulkan.h>

void CreateTexture(VulkanContext &context, ImmediateSubmit immediate_submit,
                   std::string file_name, std::string file_type,
                   Texture &texture) {
  std::array<std::pair<std::string, AllocatedImage *>, 5> texture_images;
  texture_images[0].first = (file_name + "_albedo." + file_type);
  texture_images[0].second = &texture.albedo;

  texture_images[1].first = (file_name + "_metalRoughness." + file_type);
  texture_images[1].second = &texture.metal_roughness;

  texture_images[2].first = (file_name + "_emissive." + file_type);
  texture_images[2].second = &texture.emmissive;

  texture_images[3].first = (file_name + "_AO." + file_type);
  texture_images[3].second = &texture.ambient_occlusion;

  texture_images[4].first = (file_name + "_normal." + file_type);
  texture_images[4].second = &texture.normal;

  for (uint32_t i = 0; i < texture_images.size(); i++) {
    ImageData image_data;
    LoadImageData(texture_images[i].first, image_data, false, false);

    VkExtent3D image_size = {
        static_cast<uint32_t>(image_data.width),
        static_cast<uint32_t>(image_data.height),
        1,
    };

    CreateAllocatedImageData(
        context, immediate_submit, image_data.data, image_size, VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_USAGE_SAMPLED_BIT, *texture_images[i].second);

    DestroyImageData(image_data);
  }
}

std::array<AllocatedImage, 5> Texture::ToArray() {
  std::array<AllocatedImage, 5> texture_array = {
      albedo, metal_roughness, emmissive, ambient_occlusion, normal};
  return texture_array;
}

void DestroyTexture(VulkanContext &context, Texture &texture) {
  DestroyAllocatedImage(context, texture.albedo);
  DestroyAllocatedImage(context, texture.metal_roughness);
  DestroyAllocatedImage(context, texture.emmissive);
  DestroyAllocatedImage(context, texture.ambient_occlusion);
  DestroyAllocatedImage(context, texture.normal);
}
