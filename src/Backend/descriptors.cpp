#include "descriptors.h"
#include "context.h"
#include "util.h"
#include <cstdint>
#include <vector>
#include <vulkan/vulkan_core.h>

VkDescriptorPool CreatePool(VkDevice device,
                            const DescriptorAllocatator::PoolSizes &pool_sizes,
                            uint32_t count, VkDescriptorPoolCreateFlags flags) {
  std::vector<VkDescriptorPoolSize> sizes;
  sizes.reserve(pool_sizes.sizes.size());
  for (auto size : pool_sizes.sizes) {
    sizes.push_back({
        size.first,
        uint32_t(size.second * count),
    });
  }

  VkDescriptorPoolCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  info.flags = flags;
  info.maxSets = count;
  info.poolSizeCount = (uint32_t)sizes.size();
  info.pPoolSizes = sizes.data();

  VkDescriptorPool descriptor_pool;
  VK_CHECK(vkCreateDescriptorPool(device, &info, nullptr, &descriptor_pool));
  return descriptor_pool;
}

void DescriptorAllocatator::Init(VulkanContext &context) {
  device = context.device;
}

void DescriptorAllocatator::Cleanup() {
  for (auto &pool : m_free_pools) {
    vkDestroyDescriptorPool(device, pool, nullptr);
  }
  for (auto &pool : m_used_pools) {
    vkDestroyDescriptorPool(device, pool, nullptr);
  }
}

VkDescriptorPool DescriptorAllocatator::GrabPool() {
  if (m_free_pools.size() > 0) {
    VkDescriptorPool pool = m_free_pools.back();
    m_free_pools.pop_back();
    return pool;
  } else {
    return CreatePool(device, m_descriptor_sizes, 100, 0);
  }
}

bool DescriptorAllocatator::Allocate(VkDescriptorSet *set,
                                     VkDescriptorSetLayout layout) {
  if (m_current_pool == VK_NULL_HANDLE) {
    m_current_pool = GrabPool();
    m_used_pools.push_back(m_current_pool);
  }

  VkDescriptorSetAllocateInfo alloc_info{};
  alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  alloc_info.pSetLayouts = &layout;
  alloc_info.descriptorPool = m_current_pool;
  alloc_info.descriptorSetCount = 1;

  VkResult result = vkAllocateDescriptorSets(device, &alloc_info, set);

  switch (result) {
  case VK_SUCCESS:
    return true;
  case VK_ERROR_FRAGMENTED_POOL:
  case VK_ERROR_OUT_OF_POOL_MEMORY:
    break;
  default:
    return false;
  }

  m_current_pool = GrabPool();
  m_used_pools.push_back(m_current_pool);

  result = vkAllocateDescriptorSets(device, &alloc_info, set);

  if (result == VK_SUCCESS) {
    return true;
  }

  return false;
}

void DescriptorAllocatator::ResetPools() {
  for (auto pool : m_used_pools) {
    vkResetDescriptorPool(device, pool, 0);
  }

  m_used_pools.clear();

  m_current_pool = VK_NULL_HANDLE;
}
