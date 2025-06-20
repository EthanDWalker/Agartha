#include "noise.h"
#include "Backend/allocated_image.h"
#include "Backend/buffer.h"
#include "Backend/context.h"
#include "Backend/immediate_submit.h"
#include <array>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/normalize_dot.hpp>
#include <random>

void CreateNoiseImage(VulkanContext &context,
                      DescriptorBuilder &descriptor_builder,
                      Noise &noise) {
  std::uniform_real_distribution<float> random_floats(0.0, 1.0);
  std::default_random_engine generator;

  std::array<glm::vec3, 64> kernel;
  for (uint32_t i = 0; i < 64; i++) {
    glm::vec3 sample = {
        random_floats(generator) * 2.0 - 1.0,
        random_floats(generator) * 2.0 - 1.0,
        random_floats(generator),
    };
    sample = glm::normalize(sample);
    sample *= random_floats(generator);
    float scale = (float)i / 64.0;
    scale = std::lerp(0.1f, 1.0f, scale * scale);
    sample *= scale;

    kernel[i] = sample;
  }

  CreateBufferDataAsync(context, kernel.data(),
                        sizeof(glm::vec3) * kernel.size(),
                        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, noise.kernel);

  std::array<glm::vec2, 16> noise_vectors;
  for (uint32_t i = 0; i < 16; i++) {
    glm::vec2 new_noise = {
        random_floats(generator) * 2.0 - 1.0,
        random_floats(generator) * 2.0 - 1.0,
    };
    noise_vectors[i] = new_noise;
  }

  VkSamplerCreateInfo sampler_ci{};
  sampler_ci.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  sampler_ci.magFilter = VK_FILTER_NEAREST;
  sampler_ci.minFilter = VK_FILTER_NEAREST;
  sampler_ci.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  sampler_ci.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  sampler_ci.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

  vkCreateSampler(context.device, &sampler_ci, nullptr, &noise.sampler);

  VkExtent3D image_extent = {4, 4, 1};

  CreateAllocatedImage(
      context, image_extent, VK_FORMAT_R32G32_SFLOAT,
      VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, noise.obj);

  AllocatedBuffer upload_buffer;
  CreateBuffer(context, sizeof(glm::vec2) * noise_vectors.size(),
               VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU,
               upload_buffer);

  memcpy(upload_buffer.info.pMappedData, noise_vectors.data(),
         sizeof(glm::vec2) * noise_vectors.size());

  ImmediateSubmit::SubmitAsync(context, [&](VkCommandBuffer cmd) {
    TransitionImage(cmd, VK_IMAGE_LAYOUT_UNDEFINED,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, noise.obj.image);
    VkBufferImageCopy copy_region{};
    copy_region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copy_region.imageSubresource.layerCount = 1;
    copy_region.imageExtent = noise.obj.extent;

    vkCmdCopyBufferToImage(cmd, upload_buffer.buffer, noise.obj.image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                           &copy_region);

    TransitionImage(cmd, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL, noise.obj.image);
  });

  DestroyBuffer(context, upload_buffer);
}

void DestroyNoiseImage(VulkanContext &context, Noise &noise) {
  DestroyAllocatedImage(context, noise.obj);
  DestroyBuffer(context, noise.kernel);
  DestroyImageSampler(context, noise.sampler);
}
