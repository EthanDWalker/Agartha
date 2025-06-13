#include "binding_table.h"
#include "Backend/buffer.h"
#include "Backend/context.h"
#include "Backend/pipeline.h"
#include "Backend/util.h"
#include <cstdint>
#include <span>
#include <vector>

VkPhysicalDeviceRayTracingPipelinePropertiesKHR
GetRaytracingPipelineProperties(VulkanContext &context) {
  VkPhysicalDeviceRayTracingPipelinePropertiesKHR properties{};
  properties.sType =
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;

  VkPhysicalDeviceProperties2 device_properties{};
  device_properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
  device_properties.pNext = &properties;

  vkGetPhysicalDeviceProperties2(context.physical_device, &device_properties);

  return properties;
}

void CreateShaderBindingTable(
    VulkanContext &context, Pipeline &pipeline,
    std::span<VkRayTracingShaderGroupCreateInfoKHR> shader_groups,
    ShaderBindingTable &binding_table) {

  auto properties = GetRaytracingPipelineProperties(context);

  const uint32_t handle_size = properties.shaderGroupHandleSize;
  const uint32_t handle_size_aligned = AlignedSize(
      properties.shaderGroupHandleSize, properties.shaderGroupHandleAlignment);
  const uint32_t group_count = static_cast<uint32_t>(shader_groups.size());
  const uint32_t binding_table_size = group_count * handle_size_aligned;

  assert(handle_size_aligned <= properties.maxShaderGroupStride);
  assert(handle_size_aligned % properties.shaderGroupHandleAlignment == 0);

  std::vector<uint8_t> shader_handle_storage(binding_table_size);
  VK_CHECK(vkGetRayTracingShaderGroupHandlesKHR(context.device, pipeline.obj, 0,
                                                group_count, binding_table_size,
                                                shader_handle_storage.data()));

  const VkBufferUsageFlags buffer_usage_flags =
      VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR |
      VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

  CreateBufferDataAsync(context, shader_handle_storage.data(),
                        handle_size_aligned, buffer_usage_flags,
                        binding_table.ray_gen);
  CreateBufferDataAsync(
      context, shader_handle_storage.data() + handle_size_aligned,
      handle_size_aligned, buffer_usage_flags, binding_table.miss);
  CreateBufferDataAsync(
      context, shader_handle_storage.data() + handle_size_aligned * 2,
      handle_size_aligned, buffer_usage_flags, binding_table.closest_hit);

  binding_table.ray_gen_address =
      GetDeviceAddress(context, binding_table.ray_gen.buffer);
  binding_table.miss_address =
      GetDeviceAddress(context, binding_table.miss.buffer);
  binding_table.closest_hit_address =
      GetDeviceAddress(context, binding_table.closest_hit.buffer);
}

void DestroyShaderBindingTable(VulkanContext &context,
                               ShaderBindingTable &binding_table) {
  DestroyBuffer(context, binding_table.closest_hit);
  DestroyBuffer(context, binding_table.miss);
  DestroyBuffer(context, binding_table.ray_gen);
}
