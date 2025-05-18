#include "descriptors.h"
#include "Backend/context.h"
#include "Backend/util.h"
#include "fmt/base.h"
#include <array>
#include <cassert>
#include <cstdint>
#include <vector>

void DesciptorBuilder::Init(VulkanContext &context) { pool.Init(context); }

void DesciptorBuilder::Destroy(VulkanContext &context) {
  pool.Destroy(context);
}

void DesciptorBuilder::BindBuffer(uint32_t binding, VkBuffer buffer) {
  VkDescriptorSetLayoutBinding new_binding{};
  new_binding.binding = binding;
  new_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  new_binding.descriptorCount = 1;

  bindings.push_back(new_binding);

  VkDescriptorBufferInfo buffer_write{};
  buffer_write.buffer = buffer;
  buffer_write.offset = 0;
  buffer_write.range = VK_WHOLE_SIZE;
  buffer_writes[binding] = buffer_write;
}

void DesciptorBuilder::BindImage(uint32_t binding, VkImageView image_view,
                                 VkSampler sampler) {
  VkDescriptorSetLayoutBinding new_binding{};
  new_binding.binding = binding;
  new_binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  new_binding.descriptorCount = 1;

  bindings.push_back(new_binding);

  VkDescriptorImageInfo image_write{};
  image_write.sampler = sampler;
  image_write.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  image_write.imageView = image_view;
  image_writes[binding] = image_write;
}

void DesciptorBuilder::Reset() { bindings.clear(); }

void DesciptorBuilder::Build(VulkanContext &context,
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

  vkCreateDescriptorSetLayout(context.device, &ds_layout_ci, nullptr, &layout);

  pool.Allocate(context, layout, set);

  std::vector<VkWriteDescriptorSet> binding_writes;
  for (auto &[binding, image_write] : image_writes) {
    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstBinding = binding;
    write.dstSet = set;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.pImageInfo = &image_write;

    binding_writes.push_back(write);
  }

  for (auto &[binding, buffer_write] : buffer_writes) {
    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstBinding = binding;
    write.dstSet = set;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    write.pBufferInfo = &buffer_write;

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
                              VkDescriptorSet &set) {
  VkDescriptorSetAllocateInfo ds_alloc_info{};
  ds_alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  ds_alloc_info.descriptorPool = current_pool;
  ds_alloc_info.pSetLayouts = &layout;
  ds_alloc_info.descriptorSetCount = 1;

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
