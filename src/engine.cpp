#include "engine.h"
#include "Backend/acceleration_structure.h"
#include "Backend/allocated_image.h"
#include "Backend/binding_table.h"
#include "Backend/buffer.h"
#include "Backend/context.h"
#include "Backend/descriptors.h"
#include "Backend/init.h"
#include "Backend/pipeline.h"
#include "Backend/util.h"
#include "Loaders/model.h"
#include "Managers/physics_manager.h"
#include "Managers/scene_manager.h"
#include "Managers/texture_manager.h"
#include "fmt/format.h"
#include "render_graph.h"
#include "timer.h"
#include "types.h"
#include <GLFW/glfw3.h>
#include <Volk/volk.h>
#include <array>
#include <cstdint>
#include <cstring>
#include <fmt/base.h>
#include <mutex>
#include <vector>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>
#include <glm/gtx/transform.hpp>

void Engine::CreateRenderGraph() {
  RenderGraphBuilder builder{};

  {
    builder.AddPass(0, {}, [&](VkCommandBuffer cmd) {
      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, cull_pipeline.obj);

      std::array<VkDescriptorSet, 4> ds = {
          cull_descriptor_set, camera.descriptor_set,
          scene_manager.instance_descriptor_set,
          scene_manager.object_descriptor_set};

      vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                              cull_pipeline.layout, 0, ds.size(), ds.data(), 0,
                              nullptr);

      vkCmdDispatch(cmd, std::ceil(scene_manager.instance_index / 64.0f), 1, 1);
    });

    builder.AddPass(0, {}, [&](VkCommandBuffer cmd) {
      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                        shadow_cull_pipeline.obj);

      std::array<VkDescriptorSet, 3> ds = {
          shadow_cull_descriptor_set,
          scene_manager.instance_descriptor_set,
          scene_manager.object_descriptor_set,
      };

      vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                              shadow_cull_pipeline.layout, 0, ds.size(),
                              ds.data(), 0, nullptr);

      vkCmdDispatch(cmd, std::ceil(scene_manager.instance_index / 64.0f), 1, 1);
    });
  }

  {
    DependencyBuilder shadow_pass_dep{};

    shadow_pass_dep.AddImageTransition(VK_IMAGE_LAYOUT_UNDEFINED,
                                       VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                                       shadow_image);

    builder.AddPass(1, shadow_pass_dep.dependency, [&](VkCommandBuffer cmd) {
      VkViewport viewport = vkinit::Viewport(shadow_image.extent);
      vkCmdSetViewport(cmd, 0, 1, &viewport);
      VkRect2D scissor = vkinit::Scissor(shadow_image.extent);
      vkCmdSetScissor(cmd, 0, 1, &scissor);

      VkRenderingAttachmentInfo depth = vkinit::DepthAttachmentInfo(
          shadow_image.image_view, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

      VkRenderingInfo render_info =
          vkinit::RenderingInfo(shadow_image.extent, {}, &depth);

      vkCmdBeginRendering(cmd, &render_info);

      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                        shadow_pipeline.obj);

      uint32_t draw_count = 0;
      memcpy(&draw_count, shadow_culled_draw_count_buffer.info.pMappedData,
             sizeof(uint32_t));

      if (draw_count > scene_manager.instance_index) {
        vkCmdEndRendering(cmd);
        return;
      }

      std::array<VkDescriptorSet, 4> ds = {
          shadow_descriptor_set,
          scene_manager.instance_descriptor_set,
          scene_manager.object_descriptor_set,
          light_descriptor_set,
      };

      vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                              shadow_pipeline.layout, 0, ds.size(), ds.data(),
                              0, nullptr);

      vkCmdBindIndexBuffer(cmd, scene_manager.index_buffer.buffer, 0,
                           VK_INDEX_TYPE_UINT32);

      vkCmdDrawIndexedIndirect(
          cmd, shadow_draw_indirect_buffer.buffer, 0, draw_count,
          static_cast<uint32_t>(sizeof(VkDrawIndexedIndirectCommand)));

      vkCmdEndRendering(cmd);
    });
  }

  {
    DependencyBuilder main_pass_dep{};
    main_pass_dep.AddImageTransition(VK_IMAGE_LAYOUT_UNDEFINED,
                                     VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                     main_image);
    main_pass_dep.AddImageTransition(VK_IMAGE_LAYOUT_UNDEFINED,
                                     VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                     mr_normal_image);

    main_pass_dep.AddImageTransition(VK_IMAGE_LAYOUT_UNDEFINED,
                                     VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                                     depth_image);

    main_pass_dep.AddImageTransition(VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                                     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                     shadow_image);

    builder.AddPass(2, main_pass_dep.dependency, [&](VkCommandBuffer cmd) {
      VkViewport viewport = vkinit::Viewport(main_image.extent);
      vkCmdSetViewport(cmd, 0, 1, &viewport);
      VkRect2D scissor = vkinit::Scissor(main_image.extent);
      vkCmdSetScissor(cmd, 0, 1, &scissor);

      VkClearColorValue clear_color_value{};
      clear_color_value = {0.0f, 0.0f, 0.0f, 0.0f};

      VkClearValue clear_value{};
      clear_value.color = clear_color_value;

      VkRenderingAttachmentInfo color_att =
          vkinit::AttachmentInfo(main_image.image_view, nullptr, &clear_value,
                                 VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

      VkRenderingAttachmentInfo mr_normal_att = vkinit::AttachmentInfo(
          mr_normal_image.image_view, nullptr, &clear_value,
          VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

      std::array<VkRenderingAttachmentInfo, 2> attachments = {
          color_att,
          mr_normal_att,
      };

      VkRenderingAttachmentInfo depth_att = vkinit::DepthAttachmentInfo(
          depth_image.image_view, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

      VkRenderingInfo rendering_info =
          vkinit::RenderingInfo(main_image.extent, attachments, &depth_att);

      vkCmdBeginRendering(cmd, &rendering_info);

      {
        std::lock_guard<std::mutex> lock(texture_manager.texture_mutex);

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          main_pipeline.obj);

        uint32_t draw_count = 0;
        memcpy(&draw_count, culled_draw_count_buffer.info.pMappedData,
               sizeof(uint32_t));

        if (draw_count > scene_manager.instance_index) {
          vkCmdEndRendering(cmd);
          return;
        }

        std::array<VkDescriptorSet, 6> ds = {
            main_descriptor_set,
            texture_manager.descriptor_set,
            camera.descriptor_set,
            scene_manager.object_descriptor_set,
            scene_manager.instance_descriptor_set,
            light_descriptor_set,
        };

        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                main_pipeline.layout, 0, ds.size(), ds.data(),
                                0, nullptr);

        vkCmdBindIndexBuffer(cmd, scene_manager.index_buffer.buffer, 0,
                             VK_INDEX_TYPE_UINT32);

        vkCmdDrawIndexedIndirect(
            cmd, draw_indirect_buffer.buffer, 0, draw_count,
            static_cast<uint32_t>(sizeof(VkDrawIndexedIndirectCommand)));
      }

      vkCmdEndRendering(cmd);
    });
  }

  {
    DependencyBuilder reflection_pass_dep{};
    reflection_pass_dep.AddImageTransition(
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
        main_image);

    reflection_pass_dep.AddImageTransition(
        VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
        depth_image);

    builder.AddPass(
        3, reflection_pass_dep.dependency, [&](VkCommandBuffer cmd) {
          if (ray_tracing_pipeline.obj != VK_NULL_HANDLE &&
              shader_binding_table.closest_hit_address != 0) {
            auto properties = GetRaytracingPipelineProperties(context);

            const uint32_t handle_size_aligned =
                AlignedSize(properties.shaderGroupHandleSize,
                            properties.shaderGroupHandleAlignment);

            VkStridedDeviceAddressRegionKHR raygen_entry{};
            raygen_entry.deviceAddress = shader_binding_table.ray_gen_address;
            raygen_entry.stride = handle_size_aligned;
            raygen_entry.size = handle_size_aligned;

            VkStridedDeviceAddressRegionKHR miss_entry{};
            miss_entry.deviceAddress = shader_binding_table.miss_address;
            miss_entry.stride = handle_size_aligned;
            miss_entry.size = handle_size_aligned;

            VkStridedDeviceAddressRegionKHR hit_entry{};
            hit_entry.deviceAddress = shader_binding_table.closest_hit_address;
            hit_entry.stride = handle_size_aligned;
            hit_entry.size = handle_size_aligned;

            VkStridedDeviceAddressRegionKHR callable_entry{};

            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
                              ray_tracing_pipeline.obj);

            std::array<VkDescriptorSet, 5> ds = {
                ray_tracing_descriptor_set,
                camera.descriptor_set,
                scene_manager.object_descriptor_set,
                texture_manager.descriptor_set,
                light_descriptor_set,
            };

            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
                                    ray_tracing_pipeline.layout, 0, ds.size(),
                                    ds.data(), 0, nullptr);

            vkCmdTraceRaysKHR(cmd, &raygen_entry, &miss_entry, &hit_entry,
                              &callable_entry, main_image.extent.width,
                              main_image.extent.height, 1);
          }
        });
  }

  // draw image switched with main image
  render_graph.Init(context, window);
  render_graph.render_graph = builder.render_graph;
  render_graph.root_callback = [&](VkCommandBuffer cmd, VkImage swapchain_image,
                                   VkExtent2D swapchain_extent) {
    TransitionImage(cmd, VK_IMAGE_LAYOUT_GENERAL,
                    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, main_image.image);

    TransitionImage(cmd, VK_IMAGE_LAYOUT_UNDEFINED,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, swapchain_image);

    CopyImageToImage(cmd, main_image.image, swapchain_image,
                     {main_image.extent.width, main_image.extent.height},
                     swapchain_extent);

    TransitionImage(cmd, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, swapchain_image);
  };
}

void Engine::Init() {
  glfwInit();
  glfwWindowHint(GLFW_CLIENT_API, GLFW_FALSE);
  window = glfwCreateWindow(1600, 900, "Engine", nullptr, nullptr);

  InitVulkanContext(window, DEBUG, context);

  immediate_submit.Create(context);
  descriptor_builder.Init(context);
  camera.Create(context, descriptor_builder);
  texture_manager.Init(context, descriptor_builder);
  scene_manager.Init(context, descriptor_builder);
  physics_manager.Init(context);

  VkExtent3D draw_image_extent = {
      1600,
      900,
      1,
  };

  CreateAllocatedImage(
      context, draw_image_extent, VK_FORMAT_R16G16B16A16_SFLOAT,
      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
          VK_IMAGE_USAGE_STORAGE_BIT,
      main_image);

  CreateAllocatedImage(
      context, draw_image_extent, VK_FORMAT_R16G16B16A16_SFLOAT,
      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
      mr_normal_image);

  CreateAllocatedImage(context, draw_image_extent, VK_FORMAT_D32_SFLOAT,
                       VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
                           VK_IMAGE_USAGE_STORAGE_BIT,
                       depth_image);

  CreateAllocatedImage(context, {1024 * 4, 1024 * 4, 1}, VK_FORMAT_D32_SFLOAT,
                       VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
                           VK_IMAGE_USAGE_SAMPLED_BIT,
                       shadow_image);

  VkSamplerCreateInfo shadow_sampler_ci{};
  shadow_sampler_ci.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  shadow_sampler_ci.magFilter = VK_FILTER_LINEAR;
  shadow_sampler_ci.minFilter = VK_FILTER_LINEAR;
  shadow_sampler_ci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
  shadow_sampler_ci.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  shadow_sampler_ci.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  shadow_sampler_ci.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  shadow_sampler_ci.compareEnable = VK_TRUE;
  shadow_sampler_ci.compareOp = VK_COMPARE_OP_GREATER_OR_EQUAL;
  shadow_sampler_ci.anisotropyEnable = VK_FALSE;
  shadow_sampler_ci.minLod = 0.0f;
  shadow_sampler_ci.maxLod = 1.0f;

  vkCreateSampler(context.device, &shadow_sampler_ci, nullptr, &shadow_sampler);

  CreateBufferData(context, immediate_submit, &point_light, sizeof(PointLight),
                   VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, point_light_buffer);

  CreateBufferData(context, immediate_submit, &directional_light,
                   sizeof(DirectionalLight), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                   directional_light_buffer);

  CreateBuffer(context, sizeof(glm::mat4),
               VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
                   VK_BUFFER_USAGE_TRANSFER_DST_BIT,
               VMA_MEMORY_USAGE_GPU_ONLY, light_matrix_buffer);

  CreateBuffer(context, sizeof(uint32_t), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
               VMA_MEMORY_USAGE_AUTO, culled_draw_count_buffer);

  CreateBuffer(
      context, sizeof(VkDrawIndexedIndirectCommand) * SCENE_MAX_OBJECTS,
      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
      VMA_MEMORY_USAGE_GPU_ONLY, draw_indirect_buffer);

  CreateBuffer(context, sizeof(uint32_t) * SCENE_MAX_INSTANCES,
               VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_ONLY,
               visible_instance_buffer);

  CreateBuffer(context, sizeof(uint32_t), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
               VMA_MEMORY_USAGE_AUTO, shadow_culled_draw_count_buffer);

  CreateBuffer(
      context, sizeof(VkDrawIndexedIndirectCommand) * SCENE_MAX_OBJECTS,
      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
      VMA_MEMORY_USAGE_GPU_ONLY, shadow_draw_indirect_buffer);

  CreateBuffer(context, sizeof(uint32_t) * SCENE_MAX_INSTANCES,
               VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_ONLY,
               shadow_visible_instance_buffer);

  {
    descriptor_builder.Reset();
    descriptor_builder.BindUniformBuffer(0, point_light_buffer.buffer);
    descriptor_builder.BindUniformBuffer(1, directional_light_buffer.buffer);
    descriptor_builder.BindUniformBuffer(2, light_matrix_buffer.buffer);
    descriptor_builder.Build(context, VK_SHADER_STAGE_ALL, light_descriptor_set,
                             light_descriptor_layout);
  }

  {
    descriptor_builder.Reset();
    descriptor_builder.BindStorageBuffer(0, visible_instance_buffer.buffer);
    descriptor_builder.BindCombinedImage(1, shadow_image.image_view,
                                         shadow_sampler);
    descriptor_builder.Build(
        context, VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT,
        main_descriptor_set, main_descriptor_layout);

    GraphicsPipelineBuilder pipeline_builder;
    pipeline_builder.SetShaders(context, "main_pass.vert.spv",
                                "main_pass.frag.spv");
    pipeline_builder.Default();
    pipeline_builder.AddColorAttachment(main_image.format);
    pipeline_builder.AddColorAttachment(mr_normal_image.format);
    pipeline_builder.AddDescriptorSetLayout(main_descriptor_layout);
    pipeline_builder.AddDescriptorSetLayout(
        texture_manager.descriptor_set_layout);
    pipeline_builder.AddDescriptorSetLayout(camera.descriptor_layout);
    pipeline_builder.AddDescriptorSetLayout(
        scene_manager.object_descriptor_layout);
    pipeline_builder.AddDescriptorSetLayout(
        scene_manager.instance_descriptor_layout);
    pipeline_builder.AddDescriptorSetLayout(light_descriptor_layout);
    pipeline_builder.Build(context, main_pipeline);
  }

  {
    descriptor_builder.Reset();
    descriptor_builder.BindStorageBuffer(0,
                                         shadow_visible_instance_buffer.buffer);
    descriptor_builder.Build(context, VK_SHADER_STAGE_VERTEX_BIT,
                             shadow_descriptor_set, shadow_descriptor_layout);
    GraphicsPipelineBuilder pipeline_builder;
    pipeline_builder.SetShaders(context, "shadow.vert.spv", "shadow.frag.spv");
    pipeline_builder.Default();
    pipeline_builder.SetCullMode(VK_CULL_MODE_BACK_BIT,
                                 VK_FRONT_FACE_CLOCKWISE);
    pipeline_builder.AddDescriptorSetLayout(shadow_descriptor_layout);
    pipeline_builder.AddDescriptorSetLayout(
        scene_manager.instance_descriptor_layout);
    pipeline_builder.AddDescriptorSetLayout(
        scene_manager.object_descriptor_layout);
    pipeline_builder.AddDescriptorSetLayout(light_descriptor_layout);
    pipeline_builder.Build(context, shadow_pipeline);
  }

  {
    descriptor_builder.Reset();
    descriptor_builder.BindStorageBuffer(0, draw_indirect_buffer.buffer);
    descriptor_builder.BindStorageBuffer(1, culled_draw_count_buffer.buffer);
    descriptor_builder.BindStorageBuffer(2, visible_instance_buffer.buffer);
    descriptor_builder.Build(context, VK_SHADER_STAGE_COMPUTE_BIT,
                             cull_descriptor_set, cull_descriptor_layout);
    ComputePipelineBuilder pipeline_builder{};
    pipeline_builder.SetShader(context, "frustum_cull.comp.spv");
    pipeline_builder.AddDescriptorSetLayout(cull_descriptor_layout);
    pipeline_builder.AddDescriptorSetLayout(camera.descriptor_layout);
    pipeline_builder.AddDescriptorSetLayout(
        scene_manager.instance_descriptor_layout);
    pipeline_builder.AddDescriptorSetLayout(
        scene_manager.object_descriptor_layout);
    pipeline_builder.Build(context, cull_pipeline);
  }

  {
    descriptor_builder.Reset();
    descriptor_builder.BindStorageBuffer(0, shadow_draw_indirect_buffer.buffer);
    descriptor_builder.BindStorageBuffer(
        1, shadow_culled_draw_count_buffer.buffer);
    descriptor_builder.BindStorageBuffer(2,
                                         shadow_visible_instance_buffer.buffer);
    descriptor_builder.Build(context, VK_SHADER_STAGE_COMPUTE_BIT,
                             shadow_cull_descriptor_set,
                             shadow_cull_descriptor_layout);
    ComputePipelineBuilder pipeline_builder{};
    pipeline_builder.SetShader(context, "shadow_cull.comp.spv");
    pipeline_builder.AddDescriptorSetLayout(shadow_cull_descriptor_layout);
    pipeline_builder.AddDescriptorSetLayout(
        scene_manager.instance_descriptor_layout);
    pipeline_builder.AddDescriptorSetLayout(
        scene_manager.object_descriptor_layout);
    pipeline_builder.Build(context, shadow_cull_pipeline);
  }

  std::thread([this]() {
    SCOPED_TIMER("Scene load");
    auto gltf_data = LoadModel("Sponza.gltf");

    for (auto &mesh : gltf_data) {
      scene_manager.AddObject(
          context, mesh,
          texture_manager.GetMaterial(context, mesh.material_data));
    }

    {
      descriptor_builder.Reset();
      descriptor_builder.BindAccelerationStructure(
          0, scene_manager.top_level_as.obj);
      descriptor_builder.BindStorageImage(1, main_image.image_view);
      descriptor_builder.BindStorageImage(2, mr_normal_image.image_view);
      descriptor_builder.BindStorageImage(3, depth_image.image_view);
      descriptor_builder.BindCombinedImage(4, shadow_image.image_view,
                                           shadow_sampler);
      descriptor_builder.Build(
          context,
          VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
          ray_tracing_descriptor_set, ray_tracing_descriptor_layout);

      RaytracingPipelineBuilder pipeline_builder{};
      pipeline_builder.SetShaders(context, "reflections.rgen.spv",
                                  "reflections.rmiss.spv",
                                  "reflections.rchit.spv");
      pipeline_builder.AddDescriptorSetLayout(ray_tracing_descriptor_layout);
      pipeline_builder.AddDescriptorSetLayout(camera.descriptor_layout);
      pipeline_builder.AddDescriptorSetLayout(
          scene_manager.object_descriptor_layout);
      pipeline_builder.AddDescriptorSetLayout(
          texture_manager.descriptor_set_layout);
      pipeline_builder.AddDescriptorSetLayout(light_descriptor_layout);
      const uint8_t max_recursion = 1;
      pipeline_builder.Build(context, max_recursion, ray_tracing_pipeline);

      CreateShaderBindingTable(context, ray_tracing_pipeline,
                               pipeline_builder.shader_groups,
                               shader_binding_table);

      physics_manager.SetTopLevelAS(context, scene_manager.top_level_as);
    }
  }).detach();

  CreateRenderGraph();
}

void Engine::Run() {
  const float distance = 200.0f;
  const float scene_extent = 85.0f;
  const float far_plane = 240.0f;
  const float near_plane = 0.1f;

  {
    glm::vec3 light_dir = normalize(glm::vec3(directional_light.direction));
    glm::vec3 light_pos = glm::zero<glm::vec3>() - light_dir * distance;
    glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);

    glm::mat4 light_view = glm::lookAt(light_pos, glm::zero<glm::vec3>(), up);

    glm::mat4 light_projection =
        glm::ortho(-scene_extent, scene_extent, -scene_extent, scene_extent,
                   near_plane, far_plane);

    glm::mat4 light_matrix = light_projection * light_view;

    UpdateBuffer(context, immediate_submit, &light_matrix, sizeof(glm::mat4), 0,
                 light_matrix_buffer);
  }

  bool should_close = false;

  float delta_time;
  while (!glfwWindowShouldClose(window)) {
    Timer timer{};
    glfwPollEvents();
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
      glfwSetWindowShouldClose(window, true);
      should_close = true;
    }

    if (physics_manager.tlas_set) {
      RayQuery ray_query{};
      ray_query.origin = glm::vec3(0, 0, 0);
      ray_query.direction = glm::vec3(0, -1, 0);
      ray_query.t_min = 0.001f;
      ray_query.t_max = 1000.0f;

      physics_manager.AddRayQuery(context, &ray_query);
      physics_manager.FlushRayQueries(context);
    }

    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
      // fmt::println("hi");
    }

    camera.Update(context, immediate_submit, window, delta_time);

    render_graph.Render(context);
    if (render_graph.resize_requested == true) {
      render_graph.Resize(context, window);
    }

    delta_time = timer.Elapsed();
    glfwSetWindowTitle(
        window,
        fmt::format("FPS: {}", std::round(1 / timer.Elapsed())).c_str());
  }
}

void Engine::Destroy() {
  vkDeviceWaitIdle(context.device);

  render_graph.Destroy(context);
  texture_manager.Destroy(context);
  descriptor_builder.Destroy(context);
  physics_manager.Destroy(context);
  scene_manager.Destroy(context);
  immediate_submit.Destroy(context);
  camera.Destroy(context);

  vkDestroyDescriptorSetLayout(context.device, main_descriptor_layout, nullptr);
  vkDestroyDescriptorSetLayout(context.device, shadow_descriptor_layout,
                               nullptr);
  vkDestroyDescriptorSetLayout(context.device, cull_descriptor_layout, nullptr);
  vkDestroyDescriptorSetLayout(context.device, shadow_cull_descriptor_layout,
                               nullptr);
  vkDestroyDescriptorSetLayout(context.device, ray_tracing_descriptor_layout,
                               nullptr);
  vkDestroyDescriptorSetLayout(context.device, light_descriptor_layout,
                               nullptr);

  DestroyImageSampler(context, shadow_sampler);

  DestroyShaderBindingTable(context, shader_binding_table);

  DestroyBuffer(context, point_light_buffer);
  DestroyBuffer(context, directional_light_buffer);
  DestroyBuffer(context, light_matrix_buffer);
  DestroyBuffer(context, draw_indirect_buffer);
  DestroyBuffer(context, culled_draw_count_buffer);
  DestroyBuffer(context, visible_instance_buffer);

  DestroyBuffer(context, shadow_culled_draw_count_buffer);
  DestroyBuffer(context, shadow_draw_indirect_buffer);
  DestroyBuffer(context, shadow_visible_instance_buffer);

  DestroyAllocatedImage(context, depth_image);
  DestroyAllocatedImage(context, shadow_image);
  DestroyAllocatedImage(context, main_image);
  DestroyAllocatedImage(context, mr_normal_image);

  DestroyPipeline(context, main_pipeline);
  DestroyPipeline(context, shadow_pipeline);
  DestroyPipeline(context, cull_pipeline);
  DestroyPipeline(context, shadow_cull_pipeline);
  DestroyPipeline(context, ray_tracing_pipeline);

  DestroyVulkanContext(context);

  glfwDestroyWindow(window);
  glfwTerminate();
}
