#include "descriptors.h"
#include "Backend/allocated_image.h"
#include "Backend/context.h"
#include "Backend/util.h"
#include "fmt/base.h"
#include <array>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <span>
#include <stdlib.h>
#include <vector>

void DescriptorBuilder::Init(VulkanContext &context) { pool.Init(context); }

void DescriptorBuilder::Destroy(VulkanContext &context) {
  pool.Destroy(context);
  for (auto *write : writes) {
    free(write);
  }
}

void DescriptorBuilder::BindUniformBuffer(uint32_t binding, VkBuffer buffer) {
  VkDescriptorSetLayoutBinding new_binding{};
  new_binding.binding = binding;
  new_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  new_binding.descriptorCount = 1;

  bindings.push_back(new_binding);

  VkDescriptorBufferInfo *buffer_write =
      (VkDescriptorBufferInfo *)malloc(sizeof(VkDescriptorBufferInfo));
  memset(buffer_write, 0, sizeof(VkDescriptorBufferInfo));
  buffer_write->buffer = buffer;
  buffer_write->offset = 0;
  buffer_write->range = VK_WHOLE_SIZE;

  writes.push_back(buffer_write);
}

void DescriptorBuilder::BindStorageBuffer(uint32_t binding, VkBuffer buffer) {
  VkDescriptorSetLayoutBinding new_binding{};
  new_binding.binding = binding;
  new_binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  new_binding.descriptorCount = 1;

  bindings.push_back(new_binding);

  VkDescriptorBufferInfo *buffer_write =
      (VkDescriptorBufferInfo *)malloc(sizeof(VkDescriptorBufferInfo));
  memset(buffer_write, 0, sizeof(VkDescriptorBufferInfo));
  buffer_write->buffer = buffer;
  buffer_write->offset = 0;
  buffer_write->range = VK_WHOLE_SIZE;

  writes.push_back(buffer_write);
}

void DescriptorBuilder::BindCombinedImage(uint32_t binding,
                                          VkImageView image_view,
                                          VkSampler sampler) {
  VkDescriptorSetLayoutBinding new_binding{};
  new_binding.binding = binding;
  new_binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  new_binding.descriptorCount = 1;

  bindings.push_back(new_binding);

  VkDescriptorImageInfo *image_write =
      (VkDescriptorImageInfo *)malloc(sizeof(VkDescriptorImageInfo));
  memset(image_write, 0, sizeof(VkDescriptorImageInfo));
  image_write->sampler = sampler;
  image_write->imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  image_write->imageView = image_view;
  writes.push_back(image_write);
}

void DescriptorBuilder::BindImage(uint32_t binding, VkImageView image_view) {
  VkDescriptorSetLayoutBinding new_binding{};
  new_binding.binding = binding;
  new_binding.descriptorCount = 1;
  new_binding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;

  bindings.push_back(new_binding);

  VkDescriptorImageInfo *image_write =
      (VkDescriptorImageInfo *)malloc(sizeof(VkDescriptorImageInfo));

  image_write->imageView = image_view;
  image_write->imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  image_write->sampler = VK_NULL_HANDLE;

  writes.push_back(image_write);
}

void DescriptorBuilder::BindImages(uint32_t binding,
                                   std::span<AllocatedImage> images) {
  VkDescriptorSetLayoutBinding new_binding{};
  new_binding.binding = binding;
  new_binding.descriptorCount = images.size();
  new_binding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;

  bindings.push_back(new_binding);

  VkDescriptorImageInfo *image_write_array = (VkDescriptorImageInfo *)malloc(
      sizeof(VkDescriptorImageInfo) * images.size());

  for (uint32_t i = 0; i < images.size(); i++) {
    VkDescriptorImageInfo image_write{};
    image_write.imageView = images[i].image_view;
    image_write.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    image_write.sampler = VK_NULL_HANDLE;
    image_write_array[i] = image_write;
  }

  writes.push_back(image_write_array);
}

void DescriptorBuilder::BindNullImages(uint32_t binding, uint32_t amount) {
  VkDescriptorSetLayoutBinding new_binding{};
  new_binding.binding = binding;
  new_binding.descriptorCount = amount;
  new_binding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;

  bindings.push_back(new_binding);

  writes.push_back(nullptr);
}

void DescriptorBuilder::BindStorageImages(
    uint32_t binding, std::vector<VkImageView> image_views) {
  VkDescriptorSetLayoutBinding new_binding{};
  new_binding.binding = binding;
  new_binding.descriptorCount = image_views.size();
  new_binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;

  bindings.push_back(new_binding);

  VkDescriptorImageInfo *image_write_array = (VkDescriptorImageInfo *)malloc(
      sizeof(VkDescriptorImageInfo) * image_views.size());

  for (uint32_t i = 0; i < image_views.size(); i++) {
    VkDescriptorImageInfo image_write{};
    image_write.imageView = image_views[i];
    image_write.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    image_write.sampler = VK_NULL_HANDLE;
    image_write_array[i] = image_write;
  }

  writes.push_back(image_write_array);
}

void DescriptorBuilder::BindStorageImage(uint32_t binding,
                                         VkImageView image_view) {
  VkDescriptorSetLayoutBinding new_binding{};
  new_binding.binding = binding;
  new_binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
  new_binding.descriptorCount = 1;

  bindings.push_back(new_binding);

  VkDescriptorImageInfo *image_write =
      (VkDescriptorImageInfo *)malloc(sizeof(VkDescriptorImageInfo));
  memset(image_write, 0, sizeof(VkDescriptorImageInfo));
  image_write->imageLayout = VK_IMAGE_LAYOUT_GENERAL;
  image_write->imageView = image_view;
  writes.push_back(image_write);
}

void DescriptorBuilder::BindSampler(uint32_t binding, VkSampler sampler) {
  VkDescriptorSetLayoutBinding new_binding{};
  new_binding.binding = binding;
  new_binding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
  new_binding.descriptorCount = 1;

  bindings.push_back(new_binding);

  VkDescriptorImageInfo *image_write =
      (VkDescriptorImageInfo *)malloc(sizeof(VkDescriptorImageInfo));
  memset(image_write, 0, sizeof(VkDescriptorImageInfo));
  image_write->sampler = sampler;
  image_write->imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  writes.push_back(image_write);
}

void DescriptorBuilder::Reset() {
  bindings.clear();
  for (auto *write : writes) {
    free(write);
  }
  writes.clear();
}

void DescriptorBuilder::Build(VulkanContext &context,
                              VkShaderStageFlags stage_flags,
                              VkDescriptorSet &set,
                              VkDescriptorSetLayout &layout) {
  for (auto &binding : bindings) {
    binding.stageFlags = stage_flags;
  }

  VkDescriptorSetLayoutCreateInfo ds_layout_ci{};
  ds_layout_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  ds_layout_ci.bindingCount = bindings.size();
  ds_layout_ci.pBindings = bindings.data();

  VK_CHECK(vkCreateDescriptorSetLayout(context.device, &ds_layout_ci, nullptr,
                                       &layout));

  pool.Allocate(context, layout, set);

  std::vector<VkWriteDescriptorSet> binding_writes;

  for (auto &binding : bindings) {
    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstBinding = binding.binding;
    write.dstSet = set;
    write.descriptorCount = binding.descriptorCount;
    write.descriptorType = binding.descriptorType;
    switch (binding.descriptorType) {
    case VK_DESCRIPTOR_TYPE_SAMPLER:
    case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
    case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
    case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER: {
      write.pImageInfo = (VkDescriptorImageInfo *)writes[binding.binding];
      break;
    }
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER: {
      write.pBufferInfo = (VkDescriptorBufferInfo *)writes[binding.binding];
      break;
    }
    default: {
      fmt::println("DescriptorBuilder doesnt support that format");
      abort();
    }
    }
    binding_writes.push_back(write);
  }
  vkUpdateDescriptorSets(context.device, binding_writes.size(),
                         binding_writes.data(), 0, nullptr);
}

void DescriptorPool::Init(VulkanContext &context) { NewPool(context); }

void DescriptorPool::Destroy(VulkanContext &context) {
  for (auto pool : used_pools) {
    vkDestroyDescriptorPool(context.device, pool, nullptr);
  }
}

void DescriptorPool::NewPool(VulkanContext &context) {
  std::array<VkDescriptorPoolSize, pool_ratios.size()> pool_sizes{};

  for (uint32_t i = 0; i < pool_ratios.size(); i++) {
    VkDescriptorPoolSize pool_size{};
    pool_size.type = pool_ratios[i].first;
    pool_size.descriptorCount = pool_ratios[i].second * alloc_scaler;
    pool_sizes[i] = pool_size;
  }

  VkDescriptorPoolCreateInfo descriptor_pool_ci{};
  descriptor_pool_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  descriptor_pool_ci.pPoolSizes = pool_sizes.data();
  descriptor_pool_ci.poolSizeCount = pool_sizes.size();
  descriptor_pool_ci.maxSets = 256;

  VK_CHECK(vkCreateDescriptorPool(context.device, &descriptor_pool_ci, nullptr,
                                  &current_pool));

  used_pools.push_back(current_pool);
}

void DescriptorPool::Allocate(VulkanContext &context,
                              VkDescriptorSetLayout &layout,
                              VkDescriptorSet &set, void *pNext) {
  VkDescriptorSetAllocateInfo ds_alloc_info{};
  ds_alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  ds_alloc_info.descriptorPool = current_pool;
  ds_alloc_info.pSetLayouts = &layout;
  ds_alloc_info.descriptorSetCount = 1;
  ds_alloc_info.pNext = pNext;

  VkResult result =
      vkAllocateDescriptorSets(context.device, &ds_alloc_info, &set);

  switch (result) {
  case VK_ERROR_OUT_OF_POOL_MEMORY:
  case VK_ERROR_OUT_OF_DEVICE_MEMORY:
  case VK_ERROR_FRAGMENTED_POOL:
  default: {
    return;
  }
  }

  NewPool(context);

  VK_CHECK(vkAllocateDescriptorSets(context.device, &ds_alloc_info, &set));
}
