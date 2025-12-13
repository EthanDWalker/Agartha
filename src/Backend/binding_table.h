#pragma once

#include "buffer.h"
#include "pipeline.h"
#include <span>
#include <volk.h>

struct ShaderBindingTable {
  AllocatedBuffer ray_gen;
  AllocatedBuffer miss;
  AllocatedBuffer closest_hit;

  VkDeviceAddress ray_gen_address;
  VkDeviceAddress miss_address;
  VkDeviceAddress closest_hit_address;
};

VkPhysicalDeviceRayTracingPipelinePropertiesKHR GetRaytracingPipelineProperties();

void CreateShaderBindingTable(Pipeline &pipeline,
                              std::span<VkRayTracingShaderGroupCreateInfoKHR> shader_groups,
                              ShaderBindingTable &binding_table);

void DestroyShaderBindingTable(ShaderBindingTable &binding_table);
