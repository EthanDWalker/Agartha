#include "context.h"
#include "Backend/binding_table.h"
#include "Backend/buffer.h"
#include "Backend/descriptors.h"
#include "Backend/immediate_submit.h"
#include "Backend/pipeline.h"
#include "Backend/util.h"

void InitPhysicsContext(VulkanContext &vulkan_context,
                        DescriptorBuilder &descriptor_builder,
                        VkDescriptorSetLayout as_descriptor_layout,
                        PhysicsContext &context) {
  CreateBuffer(vulkan_context, sizeof(RayCastQuery) * PHYSICS_MAX_RAY_CASTS,
               VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                   VK_BUFFER_USAGE_TRANSFER_DST_BIT,
               VMA_MEMORY_USAGE_GPU_ONLY, context.ray_cast_query_buffer);
  CreateBuffer(vulkan_context, sizeof(RayCastResult) * PHYSICS_MAX_RAY_CASTS,
               VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_AUTO,
               context.ray_cast_result_buffer);

  {
    descriptor_builder.BindStorageBuffer(0,
                                         context.ray_cast_query_buffer.buffer);
    descriptor_builder.BindStorageBuffer(1,
                                         context.ray_cast_result_buffer.buffer);
    descriptor_builder.Build(vulkan_context, VK_SHADER_STAGE_ALL,
                             context.ray_cast_descriptor_set,
                             context.ray_cast_descriptor_layout);
  }

  {
    RaytracingPipelineBuilder pipeline_builder{};
    pipeline_builder.AddDescriptorSetLayout(as_descriptor_layout);
    pipeline_builder.AddDescriptorSetLayout(context.ray_cast_descriptor_layout);
    pipeline_builder.SetShaders(vulkan_context, "Physics/ray_cast.rgen.spv",
                                "Physics/ray_cast.rmiss.spv",
                                "Physics/ray_cast.rchit.spv");
    pipeline_builder.Build(vulkan_context, 1, context.ray_cast_pipeline);

    CreateShaderBindingTable(vulkan_context, context.ray_cast_pipeline,
                             pipeline_builder.shader_groups,
                             context.ray_cast_shader_binding_table);
  }
}

uint32_t PhysicsQueueRayCast(VulkanContext &vulkan_context,
                             ImmediateSubmit &immediate_submit,
                             PhysicsContext &context,
                             RayCastQuery *ray_cast_query) {
  UpdateBuffer(vulkan_context, immediate_submit, ray_cast_query,
               sizeof(RayCastQuery),
               sizeof(RayCastQuery) * context.ray_cast_index,
               context.ray_cast_query_buffer);

  return context.ray_cast_index++;
}

void PhysicsCastRays(VkCommandBuffer cmd, VulkanContext &vulkan_context,
                     PhysicsContext &context,
                     VkDescriptorSet as_descriptor_set) {
  if (context.ray_cast_index == 0)
    return;
  auto properties = GetRaytracingPipelineProperties(vulkan_context);

  const uint32_t handle_size_aligned = AlignedSize(
      properties.shaderGroupHandleSize, properties.shaderGroupHandleAlignment);

  VkStridedDeviceAddressRegionKHR raygen_entry{};
  raygen_entry.deviceAddress =
      context.ray_cast_shader_binding_table.ray_gen_address;
  raygen_entry.stride = handle_size_aligned;
  raygen_entry.size = handle_size_aligned;

  VkStridedDeviceAddressRegionKHR miss_entry{};
  miss_entry.deviceAddress = context.ray_cast_shader_binding_table.miss_address;
  miss_entry.stride = handle_size_aligned;
  miss_entry.size = handle_size_aligned;

  VkStridedDeviceAddressRegionKHR hit_entry{};
  hit_entry.deviceAddress =
      context.ray_cast_shader_binding_table.closest_hit_address;
  hit_entry.stride = handle_size_aligned;
  hit_entry.size = handle_size_aligned;

  VkStridedDeviceAddressRegionKHR callable_entry{};

  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
                    context.ray_cast_pipeline.obj);

  std::array<VkDescriptorSet, 2> ds = {
      as_descriptor_set,
      context.ray_cast_descriptor_set,
  };

  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
                          context.ray_cast_pipeline.layout, 0, ds.size(),
                          ds.data(), 0, nullptr);

  vkCmdTraceRaysKHR(cmd, &raygen_entry, &miss_entry, &hit_entry,
                    &callable_entry, context.ray_cast_index, 1, 1);

  context.ray_cast_index = 0;
}

void DestroyPhysicsContext(VulkanContext &vulkan_context,
                           PhysicsContext &context) {
  DestroyShaderBindingTable(vulkan_context,
                            context.ray_cast_shader_binding_table);

  DestroyBuffer(vulkan_context, context.ray_cast_query_buffer);
  DestroyBuffer(vulkan_context, context.ray_cast_result_buffer);

  DestroyPipeline(vulkan_context, context.ray_cast_pipeline);

  vkDestroyDescriptorSetLayout(vulkan_context.device,
                               context.ray_cast_descriptor_layout, nullptr);
}
