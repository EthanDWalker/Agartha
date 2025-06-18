#pragma once

#include "Backend/acceleration_structure.h"
#include "Backend/binding_table.h"
#include "Backend/buffer.h"
#include "Backend/descriptors.h"
#include "Backend/pipeline.h"
#include <future>
#include <glm/vec3.hpp>

const uint32_t MAX_CONSECUTVIE_RAY_QUERIES = 10;

struct RayQuery {
  glm::vec3 origin;
  float t_min;
  glm::vec3 direction;
  float t_max;
};

struct PhysicsManager {
  ShaderBindingTable ray_query_binding_table;

  DescriptorBuilder descriptor_builder;

  std::mutex ray_query_mutex;

  // 1 based index and 0 is no result
  AllocatedBuffer ray_query_result_buffer;
  AllocatedBuffer ray_query_buffer;

  AccelerationStructure top_level_as;

  Pipeline ray_query_pipeline;

  VkDescriptorSet as_descriptor_set;
  VkDescriptorSetLayout as_descriptor_layout;

  VkDescriptorSet ray_query_descriptor_set;
  VkDescriptorSetLayout ray_query_descriptor_layout;

  uint32_t ray_query_index;
  bool tlas_set;

  void Init(VulkanContext &context);

  uint32_t AddRayQuery(VulkanContext &context, RayQuery *ray_query);

  std::future<void> FlushRayQueries(VulkanContext &context);

  void SetTopLevelAS(VulkanContext &context, AccelerationStructure &tlas);

  void Destroy(VulkanContext &context);
};
