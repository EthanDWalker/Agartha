#pragma once

#include "Backend/binding_table.h"
#include "Backend/buffer.h"
#include "Backend/context.h"
#include "Backend/descriptors.h"
#include "Backend/pipeline.h"
#include <glm/vec3.hpp>

const uint32_t PHYSICS_MAX_RAY_CASTS = 16;

struct RayCastResult {
  uint32_t instance_index;
};

struct RayCastQuery {
  glm::vec3 direction;
  float tmin;
  glm::vec3 position;
  float tmax;
};

struct PhysicsContext {
  ShaderBindingTable ray_cast_shader_binding_table;

  AllocatedBuffer ray_cast_result_buffer;
  AllocatedBuffer ray_cast_query_buffer;
  Pipeline ray_cast_pipeline;

  VkDescriptorSet ray_cast_descriptor_set;
  VkDescriptorSetLayout ray_cast_descriptor_layout;

  uint32_t ray_cast_index;
};

void InitPhysicsContext(VulkanContext &vulkan_context,
                        DescriptorBuilder &descriptor_builder,
                        VkDescriptorSetLayout as_descriptor_layout,
                        PhysicsContext &context);

uint32_t PhysicsQueueRayCast(VulkanContext &vulkan_context,
                             ImmediateSubmit &immediate_submit,
                             PhysicsContext &context,
                             RayCastQuery *ray_cast_query);

void PhysicsCastRays(VkCommandBuffer cmd, VulkanContext &vulkan_context,
                     PhysicsContext &context,
                     VkDescriptorSet as_descriptor_set);

void DestroyPhysicsContext(VulkanContext &vulkan_context,
                           PhysicsContext &context);
