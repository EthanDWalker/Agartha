#include "material.h"
#include "Backend/allocated_image.h"
#include "Backend/context.h"
#include "Backend/immediate_submit.h"
#include "Loaders/image.h"
#include <array>
#include <cstdint>
#include <vulkan/vulkan.h>

void CreateMaterial(VulkanContext &context, ImmediateSubmit immediate_submit,
                   std::string file_name, std::string file_type,
                   Material &material) {
  std::array<std::pair<std::string, AllocatedImage *>, 5> material_images;
  material_images[0].first = (file_name + "_albedo." + file_type);
  material_images[0].second = &material.albedo;

  material_images[1].first = (file_name + "_metalRoughness." + file_type);
  material_images[1].second = &material.metal_roughness;

  material_images[2].first = (file_name + "_emissive." + file_type);
  material_images[2].second = &material.emmissive;

  material_images[3].first = (file_name + "_AO." + file_type);
  material_images[3].second = &material.ambient_occlusion;

  material_images[4].first = (file_name + "_normal." + file_type);
  material_images[4].second = &material.normal;

  for (uint32_t i = 0; i < material_images.size(); i++) {
    ImageData image_data;
    LoadImageData(material_images[i].first, image_data, false, false);

    VkExtent3D image_size = {
        static_cast<uint32_t>(image_data.width),
        static_cast<uint32_t>(image_data.height),
        1,
    };

    CreateAllocatedImageData(
        context, immediate_submit, image_data.data, image_size, VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_USAGE_SAMPLED_BIT, *material_images[i].second);

    DestroyImageData(image_data);
  }
}

std::array<AllocatedImage, 5> Material::ToArray() {
  std::array<AllocatedImage, 5> material_array = {
      albedo, metal_roughness, emmissive, ambient_occlusion, normal};
  return material_array;
}

void DestroyMaterial(VulkanContext &context, Material &material) {
  DestroyAllocatedImage(context, material.albedo);
  DestroyAllocatedImage(context, material.metal_roughness);
  DestroyAllocatedImage(context, material.emmissive);
  DestroyAllocatedImage(context, material.ambient_occlusion);
  DestroyAllocatedImage(context, material.normal);
}
