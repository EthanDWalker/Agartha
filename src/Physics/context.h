#pragma once

#include "Backend/binding_table.h"
#include "Backend/buffer.h"
#include "Backend/descriptors.h"
#include "Backend/pipeline.h"
#include "Managers/event.h"
#include <glm/vec3.hpp>
#include <limits>

const uint32_t PHYSICS_MAX_RAY_CASTS = 16;

struct RayCastResult {
  uint32_t instance_index{std::numeric_limits<uint32_t>::max()};
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
  AllocatedBuffer ray_cast_result_buffer_cpu;
  AllocatedBuffer ray_cast_query_buffer;
  Pipeline ray_cast_pipeline;

  VkDescriptorSet ray_cast_descriptor_set;
  VkDescriptorSetLayout ray_cast_descriptor_layout;

  RayCastResult ray_cast_results[PHYSICS_MAX_RAY_CASTS];

  Event ray_cast_event;

  uint32_t ray_cast_index;
};

void InitPhysicsContext(DescriptorBuilder &descriptor_builder,
                        VkDescriptorSetLayout as_descriptor_layout, PhysicsContext &context);

void UpdatePhysicsContext(VkDescriptorSet as_descriptor_set, PhysicsContext &context);

uint32_t PhysicsQueueRayCast(PhysicsContext &context, RayCastQuery *ray_cast_query);

void PhysicsCastRays(VkCommandBuffer cmd, PhysicsContext &context,
                     VkDescriptorSet as_descriptor_set);

void DestroyPhysicsContext(PhysicsContext &context);
