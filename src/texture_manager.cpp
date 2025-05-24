#include "texture_manager.h"
#include "Backend/allocated_image.h"
#include "Backend/context.h"
#include "Backend/descriptors.h"
#include "Backend/immediate_submit.h"
#include "Backend/util.h"
#include "Loaders/image.h"
#include "fmt/base.h"
#include "timer.h"
#include <cmath>
#include <filesystem>
#include <future>
#include <mutex>
#include <stdlib.h>

/*
struct TextureManager {
  const uint32_t MAX_TEXTURES = 1024;

  VkDescriptorSet descriptor_set;
  VkDescriptorSetLayout descriptor_set_layout;

  std::vector<AllocatedImage> texture_data;
  std::unordered_map<std::string, uint32_t> texture_indices;

  void Init(VulkanContext &context, DescriptorBuilder &descriptor_builder);
  Material GetMaterial();
  void Destroy();
};
*/

void TextureManager::Init(VulkanContext &context,
                          DescriptorBuilder &descriptor_builder,
                          ImmediateSubmit &immediate_submit) {
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

    futures.push_back(std::async(std::launch::async, [file_name, &context, &queue_mutex]() {
      LoadedImageTask task;
      task.file_name = file_name;
      ImmediateSubmit immediate_submit{};
      immediate_submit.Create(context);

      LoadImageData(file_name, task.image_data);

      VkExtent3D image_extent = {
          static_cast<uint32_t>(task.image_data.width),
          static_cast<uint32_t>(task.image_data.height),
          1,
      };

      std::lock_guard<std::mutex> lock(queue_mutex);

      CreateAllocatedImageData(context, immediate_submit, task.image_data.data,
                               image_extent, VK_FORMAT_R8G8B8A8_UNORM,
                               VK_IMAGE_USAGE_SAMPLED_BIT, task.texture_image);

      immediate_submit.Destroy(context);

      return task;
    }));
  }

  uint32_t file_index;
  std::vector<VkDescriptorImageInfo> image_infos{};

  for (auto &future : futures) {
    LoadedImageTask image_task = future.get();

    fmt::println("task done: {}", image_task.file_name);

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

void TextureManager::Destroy(VulkanContext &context) {
  for (auto &image : texture_data) {
    DestroyAllocatedImage(context, image);
  }
  vkDestroyDescriptorSetLayout(context.device, descriptor_set_layout, nullptr);
}
