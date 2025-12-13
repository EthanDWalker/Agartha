#include "context.h"
#include "Backend/binding_table.h"
#include "Backend/buffer.h"
#include "Backend/context.h"
#include "Backend/descriptors.h"
#include "Backend/immediate_submit.h"
#include "Backend/pipeline.h"
#include "Backend/util.h"
#include <cstring>
#include <thread>

void InitPhysicsContext(DescriptorBuilder &descriptor_builder,
                        VkDescriptorSetLayout as_descriptor_layout, PhysicsContext &context) {
  CreateBuffer(sizeof(RayCastQuery) * PHYSICS_MAX_RAY_CASTS,
               VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
               VMA_MEMORY_USAGE_GPU_ONLY, context.ray_cast_query_buffer);

  for (uint32_t i = 0; i < PHYSICS_MAX_RAY_CASTS; i++) {
    context.ray_cast_results[i] = {};
  }

  CreateBufferDataAsync(context.ray_cast_results, sizeof(RayCastResult) * PHYSICS_MAX_RAY_CASTS,
                        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, context.ray_cast_result_buffer);

  CreateBuffer(sizeof(RayCastResult) * PHYSICS_MAX_RAY_CASTS,
               VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
               VMA_MEMORY_USAGE_AUTO_PREFER_HOST, context.ray_cast_result_buffer_cpu);

  context.ray_cast_event.listeners.reserve(PHYSICS_MAX_RAY_CASTS);

  {
    descriptor_builder.BindStorageBuffer(0, context.ray_cast_query_buffer.buffer);
    descriptor_builder.BindStorageBuffer(1, context.ray_cast_result_buffer.buffer);
    descriptor_builder.Build(VK_SHADER_STAGE_ALL, context.ray_cast_descriptor_set,
                             context.ray_cast_descriptor_layout);
  }

  {
    RaytracingPipelineBuilder pipeline_builder{};
    pipeline_builder.AddDescriptorSetLayout(as_descriptor_layout);
    pipeline_builder.AddDescriptorSetLayout(context.ray_cast_descriptor_layout);
    pipeline_builder.SetShaders("Physics/ray_cast.rgen.spv", "Physics/ray_cast.rmiss.spv",
                                "Physics/ray_cast.rchit.spv");
    pipeline_builder.Build(1, context.ray_cast_pipeline);

    CreateShaderBindingTable(context.ray_cast_pipeline, pipeline_builder.shader_groups,
                             context.ray_cast_shader_binding_table);
  }
}

void UpdatePhysicsContext(VkDescriptorSet as_descriptor_set, PhysicsContext &context) {
  if (context.ray_cast_index != 0) {
    std::thread([&context, as_descriptor_set]() {
      ImmediateSubmit::Submit([as_descriptor_set, &context](VkCommandBuffer cmd) {
        PhysicsCastRays(cmd, context, as_descriptor_set);
      });
      ImmediateSubmit::Submit([&context, &as_descriptor_set](VkCommandBuffer cmd) {
        VkBufferCopy2 buffer_copy{};
        buffer_copy.sType = VK_STRUCTURE_TYPE_BUFFER_COPY_2;
        buffer_copy.size = context.ray_cast_result_buffer_cpu.info.size;
        VkCopyBufferInfo2 copy_info{};
        copy_info.sType = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2;
        copy_info.dstBuffer = context.ray_cast_result_buffer_cpu.buffer;
        copy_info.srcBuffer = context.ray_cast_result_buffer.buffer;
        copy_info.pRegions = &buffer_copy;
        copy_info.regionCount = 1;
        vkCmdCopyBuffer2(cmd, &copy_info);
      });
      memcpy(context.ray_cast_results, context.ray_cast_result_buffer_cpu.info.pMappedData,
             context.ray_cast_result_buffer_cpu.info.size);
      context.ray_cast_event.TriggerAll();
      context.ray_cast_event.listeners.clear();
    }).detach();
  }
}

uint32_t PhysicsQueueRayCast(PhysicsContext &context, RayCastQuery *ray_cast_query) {
  UpdateBufferAsync(ray_cast_query, sizeof(RayCastQuery),
                    sizeof(RayCastQuery) * context.ray_cast_index, context.ray_cast_query_buffer);

  return context.ray_cast_index++;
}

void PhysicsCastRays(VkCommandBuffer cmd, PhysicsContext &context,
                     VkDescriptorSet as_descriptor_set) {
  auto properties = GetRaytracingPipelineProperties();

  const uint32_t handle_size_aligned =
      AlignedSize(properties.shaderGroupHandleSize, properties.shaderGroupHandleAlignment);

  VkStridedDeviceAddressRegionKHR raygen_entry{};
  raygen_entry.deviceAddress = context.ray_cast_shader_binding_table.ray_gen_address;
  raygen_entry.stride = handle_size_aligned;
  raygen_entry.size = handle_size_aligned;

  VkStridedDeviceAddressRegionKHR miss_entry{};
  miss_entry.deviceAddress = context.ray_cast_shader_binding_table.miss_address;
  miss_entry.stride = handle_size_aligned;
  miss_entry.size = handle_size_aligned;

  VkStridedDeviceAddressRegionKHR hit_entry{};
  hit_entry.deviceAddress = context.ray_cast_shader_binding_table.closest_hit_address;
  hit_entry.stride = handle_size_aligned;
  hit_entry.size = handle_size_aligned;

  VkStridedDeviceAddressRegionKHR callable_entry{};

  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, context.ray_cast_pipeline.obj);

  std::array<VkDescriptorSet, 2> ds = {
      as_descriptor_set,
      context.ray_cast_descriptor_set,
  };

  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
                          context.ray_cast_pipeline.layout, 0, ds.size(), ds.data(), 0, nullptr);

  vkCmdTraceRaysKHR(cmd, &raygen_entry, &miss_entry, &hit_entry, &callable_entry,
                    context.ray_cast_index, 1, 1);

  context.ray_cast_index = 0;
}

void DestroyPhysicsContext(PhysicsContext &context) {
  DestroyShaderBindingTable(context.ray_cast_shader_binding_table);

  DestroyBuffer(context.ray_cast_query_buffer);
  DestroyBuffer(context.ray_cast_result_buffer);
  DestroyBuffer(context.ray_cast_result_buffer_cpu);

  DestroyPipeline(context.ray_cast_pipeline);

  vkDestroyDescriptorSetLayout(VulkanContext::device, context.ray_cast_descriptor_layout, nullptr);
}
