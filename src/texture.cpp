#include "texture.h"
#include "Backend/context.h"
#include "Backend/image.h"
#include "Backend/immediate_submit.h"
#include "fmt/base.h"
#include <cstdint>
#include <vulkan/vulkan.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

void CreateTexture(VulkanContext &context, ImmediateSubmit immediate_submit,
                   std::string file_path, Texture &texture) {
  int32_t width, height, channel_count;
  void *data = stbi_load((texture_file_path + file_path).c_str(), &width,
                         &height, &channel_count, 4);

  if (!data) {
    return fmt::println("texture creation for {} failed :(", file_path);
  }

  VkExtent3D image_size = {
      static_cast<uint32_t>(width),
      static_cast<uint32_t>(height),
      1,
  };

  CreateAllocatedImageData(context, immediate_submit, data,
                           image_size, VK_FORMAT_R8G8B8A8_UNORM,
                           VK_IMAGE_USAGE_SAMPLED_BIT, texture.image);

  stbi_image_free(data);
}

void DestroyTexture(VulkanContext &context, Texture &texture) {
  DestroyAllocatedImage(context, texture.image);
}
