#include "descriptors.h"
#include "context.h"
#include "util.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
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

void DescriptorLayoutCache::Init(VulkanContext &context) {
  m_device = context.device;
}

void DescriptorLayoutCache::Cleanup() {
  for (auto pair : m_layout_cache) {
    vkDestroyDescriptorSetLayout(m_device, pair.second, nullptr);
  }
}

VkDescriptorSetLayout DescriptorLayoutCache::CreateDescriptorLayout(
    VkDescriptorSetLayoutCreateInfo *info) {
  DescriptorLayoutInfo layout_info{};
  layout_info.bindings.reserve(info->bindingCount);
  bool is_sorted = true;
  int32_t last_binding = -1;

  for (int32_t i = 0; i < info->bindingCount; i++) {
    layout_info.bindings.push_back(info->pBindings[i]);

    if (info->pBindings[i].binding > last_binding) {
      last_binding = info->pBindings[i].binding;
    } else {
      is_sorted = false;
    }
  }

  if (!is_sorted) {
    std::sort(
        layout_info.bindings.begin(), layout_info.bindings.end(),
        [](VkDescriptorSetLayoutBinding &a, VkDescriptorSetLayoutBinding &b) {
          return a.binding < b.binding;
        });
  }

  auto it = m_layout_cache.find(layout_info);
  if (it != m_layout_cache.end()) {
    return (*it).second;
  } else {
    VkDescriptorSetLayout layout;
    vkCreateDescriptorSetLayout(m_device, info, nullptr, &layout);

    m_layout_cache[layout_info] = layout;
    return layout;
  }
}

bool DescriptorLayoutCache::DescriptorLayoutInfo::operator==(
    const DescriptorLayoutInfo &other) const {
  if (other.bindings.size() != bindings.size()) {
    return false;
  } else {
    for (int32_t i = 0; i < bindings.size(); i++) {
      if (other.bindings[i].binding != other.bindings[i].binding) {
        return false;
      }
      if (other.bindings[i].descriptorCount !=
          other.bindings[i].descriptorCount) {
        return false;
      }
      if (other.bindings[i].descriptorType !=
          other.bindings[i].descriptorType) {
        return false;
      }
      if (other.bindings[i].stageFlags != other.bindings[i].stageFlags) {
        return false;
      }
    }
    return true;
  }
}

size_t DescriptorLayoutCache::DescriptorLayoutInfo::hash() const {
  using std::hash;
  using std::size_t;

  size_t result = hash<size_t>()(bindings.size());

  for (const VkDescriptorSetLayoutBinding &b : bindings) {
    size_t binding_hash = b.binding | b.descriptorType << 8 |
                          b.descriptorCount << 16 | b.stageFlags << 24;

    result ^= hash<size_t>()(binding_hash);
  }

  return result;
}

DescriptorBuilder DescriptorBuilder::Begin(DescriptorLayoutCache *layout_cache,
                                           DescriptorAllocatator *allocator) {
  DescriptorBuilder builder;
  builder.m_allocator = allocator;
  builder.m_cache = layout_cache;
  return builder;
}

DescriptorBuilder &DescriptorBuilder::BindBuffer(
    uint32_t binding, VkDescriptorBufferInfo *buffer_info,
    VkDescriptorType type, VkShaderStageFlags stage_flags) {
  VkDescriptorSetLayoutBinding new_binding{};
  new_binding.descriptorCount = 1;
  new_binding.descriptorType = type;
  new_binding.stageFlags = stage_flags;
  new_binding.binding = binding;

  m_bindings.push_back(new_binding);

  VkWriteDescriptorSet new_write{};
  new_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  new_write.descriptorCount = 1;
  new_write.descriptorType = type;
  new_write.pBufferInfo = buffer_info;
  new_write.dstBinding = binding;

  m_writes.push_back(new_write);

  return *this;
}

bool DescriptorBuilder::Build(VkDescriptorSet &set,
                              VkDescriptorSetLayout &layout) {
  VkDescriptorSetLayoutCreateInfo layout_ci{};
  layout_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  layout_ci.pBindings = m_bindings.data();
  layout_ci.bindingCount = m_bindings.size();

  layout = m_cache->CreateDescriptorLayout(&layout_ci);

  bool success = m_allocator->Allocate(&set, layout);
  if (!success) {
    return false;
  }

  for (VkWriteDescriptorSet &write : m_writes) {
    write.dstSet = set;
  }

  vkUpdateDescriptorSets(m_allocator->device, m_writes.size(), m_writes.data(),
                         0, nullptr);

  return true;
}
