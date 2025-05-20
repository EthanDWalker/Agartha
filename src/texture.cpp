#include "texture.h"
#include "Backend/context.h"
#include "Backend/image.h"
#include "Backend/immediate_submit.h"
#include "fmt/base.h"
#include <array>
#include <cstdint>
#include <vulkan/vulkan.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

void CreateTexture(VulkanContext &context, ImmediateSubmit immediate_submit,
                   std::string file_name, std::string file_type,
                   Texture &texture) {
  std::array<std::pair<std::string, AllocatedImage *>, 3> texture_images;
  texture_images[0].first = (file_name + "_albedo." + file_type);
  texture_images[0].second = &texture.albedo;

  texture_images[1].first = (file_name + "_specular." + file_type);
  texture_images[1].second = &texture.specular;

  texture_images[2].first = (file_name + "_immission." + file_type);
  texture_images[2].second = &texture.immission;

  for (uint32_t i = 0; i < texture_images.size(); i++) {
    int32_t width, height, channel_count;
    void *data =
        stbi_load((texture_file_path + texture_images[i].first).c_str(), &width,
                  &height, &channel_count, 4);

    if (!data) {
      return fmt::println("texture creation for {} failed :( : {}",
                          texture_images[i].first, stbi_failure_reason());
    }

    VkExtent3D image_size = {
        static_cast<uint32_t>(width),
        static_cast<uint32_t>(height),
        1,
    };

    CreateAllocatedImageData(
        context, immediate_submit, data, image_size, VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_USAGE_SAMPLED_BIT, *texture_images[i].second);

    stbi_image_free(data);
  }
}

std::array<AllocatedImage, 3> Texture::ToArray() {
  std::array<AllocatedImage, 3> texture_array = {albedo, specular, immission};
  return texture_array;
}

void DestroyTexture(VulkanContext &context, Texture &texture) {
  DestroyAllocatedImage(context, texture.albedo);
  DestroyAllocatedImage(context, texture.specular);
  DestroyAllocatedImage(context, texture.immission);
}
