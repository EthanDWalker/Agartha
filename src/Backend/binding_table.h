#pragma once

#include "Backend/buffer.h"
#include "Backend/context.h"
#include "Backend/pipeline.h"
#include <Volk/volk.h>
#include <span>

struct ShaderBindingTable {
  AllocatedBuffer ray_gen;
  AllocatedBuffer miss;
  AllocatedBuffer closest_hit;

  VkDeviceAddress ray_gen_address;
  VkDeviceAddress miss_address;
  VkDeviceAddress closest_hit_address;
};

VkPhysicalDeviceRayTracingPipelinePropertiesKHR
GetRaytracingPipelineProperties(VulkanContext &context);

void CreateShaderBindingTable(
    VulkanContext &context, Pipeline &pipeline,
    std::span<VkRayTracingShaderGroupCreateInfoKHR> shader_groups,
    ShaderBindingTable &binding_table);

void DestroyShaderBindingTable(VulkanContext &context,
                               ShaderBindingTable &binding_table);
