#include "physics_manager.h"
#include "Backend/immediate_submit.h"
#include "Backend/util.h"
#include <future>
#include <mutex>

void PhysicsManager::Init(VulkanContext &context) {
  descriptor_builder.Init(context);

  CreateBuffer(context, sizeof(RayQuery) * MAX_CONSECUTVIE_RAY_QUERIES,
               VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
               VMA_MEMORY_USAGE_GPU_ONLY, ray_query_buffer);

  CreateBuffer(context, sizeof(uint32_t) * MAX_CONSECUTVIE_RAY_QUERIES,
               VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
               VMA_MEMORY_USAGE_GPU_ONLY, ray_query_result_buffer);

  descriptor_builder.Reset();
  descriptor_builder.BindStorageBuffer(0, ray_query_buffer.buffer);
  descriptor_builder.BindStorageBuffer(1, ray_query_result_buffer.buffer);
  descriptor_builder.Build(context, VK_SHADER_STAGE_ALL,
                           ray_query_descriptor_set,
                           ray_query_descriptor_layout);
}

void PhysicsManager::SetTopLevelAS(VulkanContext &context,
                                   AccelerationStructure &tlas) {
  if (tlas_set) {
    vkDestroyDescriptorSetLayout(context.device, as_descriptor_layout, nullptr);
    DestroyPipeline(context, ray_query_pipeline);
  }

  top_level_as = tlas;
  descriptor_builder.Reset();
  descriptor_builder.BindAccelerationStructure(0, tlas.obj);
  descriptor_builder.Build(context, VK_SHADER_STAGE_ALL, as_descriptor_set,
                           as_descriptor_layout);

  RaytracingPipelineBuilder pipeline_builder{};
  pipeline_builder.SetShaders(context, "ray_query.rgen.spv",
                              "ray_query.rmiss.spv", "ray_query.rchit.spv");
  const uint32_t max_recursion = 1;
  pipeline_builder.AddDescriptorSetLayout(as_descriptor_layout);
  pipeline_builder.AddDescriptorSetLayout(ray_query_descriptor_layout);
  pipeline_builder.Build(context, max_recursion, ray_query_pipeline);

  CreateShaderBindingTable(context, ray_query_pipeline,
                           pipeline_builder.shader_groups,
                           ray_query_binding_table);
  tlas_set = true;
}

std::future<void> PhysicsManager::FlushRayQueries(VulkanContext &context) {
  std::future<void> future = std::async(std::launch::async, [&]() {
    auto properties = GetRaytracingPipelineProperties(context);

    const uint32_t handle_size_aligned =
        AlignedSize(properties.shaderGroupHandleSize,
                    properties.shaderGroupHandleAlignment);

    VkStridedDeviceAddressRegionKHR raygen_entry{};
    raygen_entry.deviceAddress = ray_query_binding_table.ray_gen_address;
    raygen_entry.stride = handle_size_aligned;
    raygen_entry.size = handle_size_aligned;

    VkStridedDeviceAddressRegionKHR miss_entry{};
    miss_entry.deviceAddress = ray_query_binding_table.miss_address;
    miss_entry.stride = handle_size_aligned;
    miss_entry.size = handle_size_aligned;

    VkStridedDeviceAddressRegionKHR hit_entry{};
    hit_entry.deviceAddress = ray_query_binding_table.closest_hit_address;
    hit_entry.stride = handle_size_aligned;
    hit_entry.size = handle_size_aligned;

    VkStridedDeviceAddressRegionKHR callable_entry{};

    std::lock_guard<std::mutex> lock(ray_query_mutex);

    ImmediateSubmit::SubmitAsync(context, [=, this](VkCommandBuffer cmd) {
      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
                        ray_query_pipeline.obj);

      std::array<VkDescriptorSet, 2> ds = {
          as_descriptor_set,
          ray_query_descriptor_set,
      };

      vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
                              ray_query_pipeline.layout, 0, ds.size(),
                              ds.data(), 0, nullptr);

      vkCmdTraceRaysKHR(cmd, &raygen_entry, &miss_entry, &hit_entry,
                        &callable_entry, ray_query_index, 1, 1);
    });

    ray_query_index = 0;
  });

  return future;
}

uint32_t PhysicsManager::AddRayQuery(VulkanContext &context,
                                     RayQuery *ray_query) {
  assert(tlas_set &&
         "You must set the tlas before querying any rays you fucking idiot");

  std::lock_guard<std::mutex> lock(ray_query_mutex);

  UpdateBufferAsync(context, ray_query, sizeof(RayQuery),
                    ray_query_index * sizeof(RayQuery), ray_query_buffer);

  return ray_query_index++;
}

void PhysicsManager::Destroy(VulkanContext &context) {
  descriptor_builder.Destroy(context);

  vkDestroyDescriptorSetLayout(context.device, as_descriptor_layout, nullptr);
  vkDestroyDescriptorSetLayout(context.device, ray_query_descriptor_layout,
                               nullptr);

  DestroyShaderBindingTable(context, ray_query_binding_table);
  DestroyPipeline(context, ray_query_pipeline);
  DestroyBuffer(context, ray_query_buffer);
  DestroyBuffer(context, ray_query_result_buffer);
}
