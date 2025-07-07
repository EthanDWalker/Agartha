#include "engine.h"
#include "Backend/allocated_image.h"
#include "Backend/binding_table.h"
#include "Backend/buffer.h"
#include "Backend/context.h"
#include "Backend/descriptors.h"
#include "Backend/immediate_submit.h"
#include "Backend/init.h"
#include "Backend/pipeline.h"
#include "Loaders/model.h"
#include "Managers/light_manager.h"
#include "Managers/scene_manager.h"
#include "Managers/texture_manager.h"
#include "Physics/context.h"
#include "UI/context.h"
#include "UI/render.h"
#include "fmt/format.h"
#include "render_graph.h"
#include "timer.h"
#include <GLFW/glfw3.h>
#include <array>
#include <cstdint>
#include <cstring>
#include <fmt/base.h>
#include <vector>
#include <volk.h>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>
#include <glm/gtx/transform.hpp>

void Engine::CreateRenderGraph() {
  RenderGraphBuilder builder{};

  {
    builder.AddPass(0, {}, [&](VkCommandBuffer cmd) {
      if (scene_manager.instance_index == 0)
        return;
      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, cull_pipeline.obj);

      std::array<VkDescriptorSet, 4> ds = {
          cull_descriptor_set,
          camera.descriptor_set,
          scene_manager.instance_descriptor_set,
          scene_manager.object_descriptor_set,
      };

      vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                              cull_pipeline.layout, 0, ds.size(), ds.data(), 0,
                              nullptr);

      vkCmdDispatch(cmd, std::ceil(scene_manager.instance_index / 64.0f), 1, 1);
    });

    builder.AddPass(0, {}, [&](VkCommandBuffer cmd) {
      if (scene_manager.instance_index == 0)
        return;
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

    builder.AddPass(0, {}, [&](VkCommandBuffer cmd) {
      scene_svo.BuildDrawCommands(cmd, scene_manager);
    });
  }

  {
    DependencyBuilder depth_pass_dep{};

    depth_pass_dep.AddImageDependency(
        depth_image, {}, VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, {},
        VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, true);

    depth_pass_dep.AddBufferDependency(
        culled_draw_count_buffer, VK_ACCESS_2_SHADER_WRITE_BIT,
        VK_ACCESS_2_HOST_READ_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_2_HOST_BIT);

    depth_pass_dep.AddBufferDependency(
        draw_indirect_buffer, VK_ACCESS_2_SHADER_WRITE_BIT,
        VK_ACCESS_2_SHADER_READ_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT);

    builder.AddPass(1, depth_pass_dep.dependency, [&](VkCommandBuffer cmd) {
      uint32_t draw_count = 0;
      memcpy(&draw_count, culled_draw_count_buffer.info.pMappedData,
             sizeof(uint32_t));
      if (draw_count > scene_manager.instance_index || draw_count == 0) {
        return;
      }

      VkViewport viewport = vkinit::Viewport(depth_image.extent);
      vkCmdSetViewport(cmd, 0, 1, &viewport);
      VkRect2D scissor = vkinit::Scissor(depth_image.extent);
      vkCmdSetScissor(cmd, 0, 1, &scissor);

      VkRenderingAttachmentInfo depth_att = vkinit::DepthAttachmentInfo(
          depth_image.image_view, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
          VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE);

      VkRenderingInfo render_info =
          vkinit::RenderingInfo(depth_image.extent, {}, &depth_att);

      vkCmdBeginRendering(cmd, &render_info);

      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                        depth_pipeline.obj);

      std::array<VkDescriptorSet, 4> ds = {
          cull_descriptor_set,
          scene_manager.instance_descriptor_set,
          scene_manager.object_descriptor_set,
          camera.descriptor_set,
      };

      vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                              depth_pipeline.layout, 0, ds.size(), ds.data(), 0,
                              nullptr);

      vkCmdBindIndexBuffer(cmd, scene_manager.index_buffer.buffer, 0,
                           VK_INDEX_TYPE_UINT32);

      vkCmdDrawIndexedIndirect(
          cmd, draw_indirect_buffer.buffer, 0, draw_count,
          static_cast<uint32_t>(sizeof(VkDrawIndexedIndirectCommand)));

      vkCmdEndRendering(cmd);
    });
  }

  {
    builder.AddPass(1, {}, [&](VkCommandBuffer cmd) {
      uint32_t draw_count = 0;

      memcpy(&draw_count, shadow_culled_draw_count_buffer.info.pMappedData,
             sizeof(uint32_t));

      if (draw_count > scene_manager.instance_index || draw_count == 0)
        return;

      VkViewport viewport = vkinit::Viewport(SHADOW_IMAGE_EXTENT);
      vkCmdSetViewport(cmd, 0, 1, &viewport);
      VkRect2D scissor = vkinit::Scissor(SHADOW_IMAGE_EXTENT);
      vkCmdSetScissor(cmd, 0, 1, &scissor);

      for (uint32_t i = 0; i < light_manager.matrix_index; i++) {
        AllocatedImage shadow_image = light_manager.shadow_images[i];

        TransitionImage(cmd, {}, VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                        {}, VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT, {},
                        VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                        shadow_image.image, true);

        VkRenderingAttachmentInfo depth = vkinit::DepthAttachmentInfo(
            shadow_image.image_view, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
            VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE);

        VkRenderingInfo render_info =
            vkinit::RenderingInfo(SHADOW_IMAGE_EXTENT, {}, &depth);

        vkCmdBeginRendering(cmd, &render_info);

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          shadow_pipeline.obj);

        vkCmdPushConstants(cmd, shadow_pipeline.layout,
                           VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(uint32_t), &i);

        std::array<VkDescriptorSet, 3> ds = {
            scene_manager.instance_descriptor_set,
            scene_manager.object_descriptor_set,
            light_manager.shadow_descriptor_set,
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

        TransitionImage(cmd, VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                        VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                        VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
                        VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                        VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                        shadow_image.image, true);
      }
    });
  }

  builder.AddPass(2, {}, [&](VkCommandBuffer cmd) {
    scene_svo.Build(cmd, scene_manager, texture_manager, light_manager, camera);
  });

  {
    DependencyBuilder main_pass_dep{};
    main_pass_dep.AddImageDependency(
        main_image, {}, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, {},
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    main_pass_dep.AddImageDependency(
        mr_normal_image, {}, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, {},
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    main_pass_dep.AddImageDependency(
        depth_image, {}, VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, {},
        VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, true);

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
          depth_image.image_view, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
          VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_STORE_OP_STORE);

      VkRenderingInfo rendering_info =
          vkinit::RenderingInfo(main_image.extent, attachments, &depth_att);

      vkCmdBeginRendering(cmd, &rendering_info);

      {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          main_pipeline.obj);

        uint32_t draw_count = 0;
        memcpy(&draw_count, culled_draw_count_buffer.info.pMappedData,
               sizeof(uint32_t));

        if (draw_count > scene_manager.instance_index) {
          vkCmdEndRendering(cmd);
          return;
        }

        std::array<VkDescriptorSet, 7> ds = {
            texture_manager.descriptor_set,
            camera.descriptor_set,
            scene_manager.object_descriptor_set,
            scene_manager.instance_descriptor_set,
            light_manager.light_descriptor_set,
            light_manager.shadow_descriptor_set,
            scene_svo.svo_descriptor_set,
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
    DependencyBuilder pp_pass_dep{};
    pp_pass_dep.AddImageDependency(
        ao_image, {}, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT, {},
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_GENERAL);

    pp_pass_dep.AddImageDependency(
        main_image, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        VK_ACCESS_2_SHADER_READ_BIT,
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);

    pp_pass_dep.AddImageDependency(
        mr_normal_image, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        VK_ACCESS_2_SHADER_READ_BIT,
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);

    pp_pass_dep.AddImageDependency(
        depth_image, VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        VK_ACCESS_2_SHADER_READ_BIT,
        VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
        true);

    builder.AddPass(3, pp_pass_dep.dependency, [&](VkCommandBuffer cmd) {
      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                        ambient_occlusion_pipeline.obj);

      std::array<VkDescriptorSet, 2> ds = {gbuffer_descriptor_set,
                                           camera.descriptor_set};

      vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                              ambient_occlusion_pipeline.layout, 0, ds.size(),
                              ds.data(), 0, nullptr);

      vkCmdDispatch(cmd, std::ceil(ao_image.extent.width / 16.0f),
                    std::ceil(ao_image.extent.height / 16.0f), 1);
    });
  }

  {
    DependencyBuilder ao_upscale_dep{};

    ao_upscale_dep.AddImageDependency(ao_image, VK_ACCESS_2_SHADER_WRITE_BIT,
                                      VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                                      VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                      VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                      VK_IMAGE_LAYOUT_GENERAL,
                                      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    builder.AddPass(4, ao_upscale_dep.dependency, [&](VkCommandBuffer cmd) {
      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                        upscale_ao_pipeline.obj);

      std::array<VkDescriptorSet, 1> ds = {
          gbuffer_descriptor_set,
      };

      vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                              upscale_ao_pipeline.layout, 0, ds.size(),
                              ds.data(), 0, nullptr);

      vkCmdDispatch(cmd, std::ceil(ao_image.extent.width / 16.0f),
                    std::ceil(ao_image.extent.height / 16.0f), 1);
    });
  }

  {
    builder.AddPass(5, {}, [&](VkCommandBuffer cmd) {
      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                        tone_map_pipeline.obj);

      std::array<VkDescriptorSet, 1> ds = {
          gbuffer_descriptor_set,
      };

      vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                              tone_map_pipeline.layout, 0, ds.size(), ds.data(),
                              0, nullptr);

      vkCmdDispatch(cmd, std::ceil(main_image.extent.width / 16.0f),
                    std::ceil(main_image.extent.height / 16.0f), 1);
    });

    builder.AddPass(5, {}, [&](VkCommandBuffer cmd) {
      if (!voxel_gi_debug)
        return;
      scene_svo.DrawDebugView(cmd, camera, main_image, depth_image,
                              voxel_debug_mip_level);

      TransitionImage(cmd, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                      VK_ACCESS_2_SHADER_READ_BIT,
                      VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                      VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                      VK_IMAGE_LAYOUT_GENERAL, main_image.image);
    });
  }

  {
    DependencyBuilder ui_pass_dep{};
    ui_pass_dep.AddImageDependency(
        main_image, VK_ACCESS_2_SHADER_READ_BIT,
        VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    ui_pass_dep.AddImageDependency(
        depth_image, VK_ACCESS_2_SHADER_READ_BIT,
        VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT, VK_IMAGE_LAYOUT_GENERAL,
        VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, true);

    builder.AddPass(6, ui_pass_dep.dependency,
                    [&](VkCommandBuffer cmd) { RenderUi(cmd, main_image); });
  }

  render_graph.Init(vulkan_context, window);
  render_graph.render_graph = builder.render_graph;
  render_graph.root_callback = [&](VkCommandBuffer cmd, VkImage swapchain_image,
                                   VkExtent2D swapchain_extent) {
    TransitionImage(cmd, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                    VK_ACCESS_2_TRANSFER_READ_BIT,
                    VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                    VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, main_image.image);

    TransitionImage(cmd, {}, VK_ACCESS_2_TRANSFER_WRITE_BIT, {},
                    VK_PIPELINE_STAGE_2_TRANSFER_BIT, {},
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, swapchain_image);

    CopyImageToImage(cmd, main_image.image, swapchain_image,
                     {main_image.extent.width, main_image.extent.height},
                     swapchain_extent);

    TransitionImage(cmd, VK_ACCESS_2_TRANSFER_WRITE_BIT, {},
                    VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                    VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, swapchain_image);
  };
}

void Engine::Init() {
  glfwInit();
  glfwWindowHint(GLFW_CLIENT_API, GLFW_FALSE);
  VkExtent2D window_size = {1600 * 2, 900 * 2};
  window = glfwCreateWindow(window_size.width, window_size.height, "Engine",
                            nullptr, nullptr);

  InitVulkanContext(window, vulkan_context);

  immediate_submit.Create(vulkan_context);

  descriptor_builder.Init(vulkan_context);
  texture_manager.Init(vulkan_context, descriptor_builder);
  scene_manager.Init(vulkan_context, descriptor_builder);
  light_manager.Init(vulkan_context, descriptor_builder);

  InitPhysicsContext(vulkan_context, descriptor_builder,
                     scene_manager.as_descriptor_layout, physics_context);

  camera.Create(vulkan_context, descriptor_builder);

  scene_svo.Create(vulkan_context, scene_manager, light_manager,
                   texture_manager, camera, descriptor_builder);

  light_manager.AddDirectionalLight(vulkan_context, glm::vec3(1.0),
                                    glm::vec3(-1.0, -4.0, -1.0), 40.0,
                                    immediate_submit);

  VkExtent3D draw_image_extent = {
      window_size.width,
      window_size.height,
      1,
  };

  CreateAllocatedImage(
      vulkan_context, draw_image_extent, VK_FORMAT_R16G16B16A16_SFLOAT,
      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
          VK_IMAGE_USAGE_STORAGE_BIT,
      main_image);

  CreateAllocatedImage(vulkan_context, draw_image_extent, VK_FORMAT_D32_SFLOAT,
                       VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
                           VK_IMAGE_USAGE_STORAGE_BIT,
                       depth_image);

  CreateUiContext(vulkan_context, window, &main_image.format);

  CreateAllocatedImage(
      vulkan_context, draw_image_extent, VK_FORMAT_R8G8B8A8_UNORM,
      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
      mr_normal_image);

  VkExtent3D ao_draw_image_extent = {
      draw_image_extent.width / 2,
      draw_image_extent.height / 2,
      1,
  };

  CreateAllocatedImage(vulkan_context, ao_draw_image_extent, VK_FORMAT_R8_UNORM,
                       VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                       ao_image);

  CreateBuffer(vulkan_context, sizeof(uint32_t),
               VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_AUTO,
               culled_draw_count_buffer);

  CreateBuffer(
      vulkan_context, sizeof(VkDrawIndexedIndirectCommand) * SCENE_MAX_OBJECTS,
      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
      VMA_MEMORY_USAGE_GPU_ONLY, draw_indirect_buffer);

  CreateBuffer(vulkan_context, sizeof(uint32_t),
               VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_AUTO,
               shadow_culled_draw_count_buffer);

  CreateBuffer(
      vulkan_context, sizeof(VkDrawIndexedIndirectCommand) * SCENE_MAX_OBJECTS,
      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
      VMA_MEMORY_USAGE_GPU_ONLY, shadow_draw_indirect_buffer);

  CreateImageSampler(vulkan_context, sampler);

  {
    GraphicsPipelineBuilder pipeline_builder;
    pipeline_builder.SetShaders(vulkan_context, "main_pass.vert.spv",
                                "main_pass.frag.spv");
    pipeline_builder.Default();
    pipeline_builder.AddColorAttachment(main_image.format);
    pipeline_builder.AddColorAttachment(mr_normal_image.format);
    pipeline_builder.AddDescriptorSetLayout(
        texture_manager.descriptor_set_layout);
    pipeline_builder.AddDescriptorSetLayout(camera.descriptor_layout);
    pipeline_builder.AddDescriptorSetLayout(
        scene_manager.object_descriptor_layout);
    pipeline_builder.AddDescriptorSetLayout(
        scene_manager.instance_descriptor_layout);
    pipeline_builder.AddDescriptorSetLayout(
        light_manager.light_descriptor_layout);
    pipeline_builder.AddDescriptorSetLayout(
        light_manager.shadow_descriptor_layout);
    pipeline_builder.AddDescriptorSetLayout(scene_svo.svo_descriptor_layout);
    pipeline_builder.Build(vulkan_context, main_pipeline);
  }

  {
    GraphicsPipelineBuilder pipeline_builder;
    pipeline_builder.SetShaders(vulkan_context, "shadow.vert.spv",
                                "none.frag.spv");
    pipeline_builder.Default();
    pipeline_builder.SetCullMode(VK_CULL_MODE_BACK_BIT,
                                 VK_FRONT_FACE_CLOCKWISE);
    pipeline_builder.AddDescriptorSetLayout(
        scene_manager.instance_descriptor_layout);
    pipeline_builder.AddDescriptorSetLayout(
        scene_manager.object_descriptor_layout);
    pipeline_builder.AddDescriptorSetLayout(
        light_manager.shadow_descriptor_layout);

    pipeline_builder.AddPushConstantRange(VK_SHADER_STAGE_VERTEX_BIT,
                                          sizeof(uint32_t));

    pipeline_builder.Build(vulkan_context, shadow_pipeline);
  }

  {
    descriptor_builder.BindStorageBuffer(0, draw_indirect_buffer.buffer);
    descriptor_builder.BindStorageBuffer(1, culled_draw_count_buffer.buffer);
    descriptor_builder.Build(vulkan_context,
                             VK_SHADER_STAGE_COMPUTE_BIT |
                                 VK_SHADER_STAGE_VERTEX_BIT,
                             cull_descriptor_set, cull_descriptor_layout);
    ComputePipelineBuilder pipeline_builder{};
    pipeline_builder.SetShader(vulkan_context, "frustum_cull.comp.spv");
    pipeline_builder.AddDescriptorSetLayout(cull_descriptor_layout);
    pipeline_builder.AddDescriptorSetLayout(camera.descriptor_layout);
    pipeline_builder.AddDescriptorSetLayout(
        scene_manager.instance_descriptor_layout);
    pipeline_builder.AddDescriptorSetLayout(
        scene_manager.object_descriptor_layout);
    pipeline_builder.Build(vulkan_context, cull_pipeline);
  }

  {
    GraphicsPipelineBuilder pipeline_builder{};
    pipeline_builder.SetShaders(vulkan_context, "depth_pass.vert.spv",
                                "none.frag.spv");
    pipeline_builder.Default();
    pipeline_builder.AddDescriptorSetLayout(cull_descriptor_layout);
    pipeline_builder.AddDescriptorSetLayout(
        scene_manager.instance_descriptor_layout);
    pipeline_builder.AddDescriptorSetLayout(
        scene_manager.object_descriptor_layout);
    pipeline_builder.AddDescriptorSetLayout(camera.descriptor_layout);
    pipeline_builder.Build(vulkan_context, depth_pipeline);
  }

  {
    descriptor_builder.BindStorageBuffer(0, shadow_draw_indirect_buffer.buffer);
    descriptor_builder.BindStorageBuffer(
        1, shadow_culled_draw_count_buffer.buffer);
    descriptor_builder.Build(vulkan_context, VK_SHADER_STAGE_COMPUTE_BIT,
                             shadow_cull_descriptor_set,
                             shadow_cull_descriptor_layout);
    ComputePipelineBuilder pipeline_builder{};
    pipeline_builder.SetShader(vulkan_context, "shadow_cull.comp.spv");
    pipeline_builder.AddDescriptorSetLayout(shadow_cull_descriptor_layout);
    pipeline_builder.AddDescriptorSetLayout(
        scene_manager.instance_descriptor_layout);
    pipeline_builder.AddDescriptorSetLayout(
        scene_manager.object_descriptor_layout);
    pipeline_builder.Build(vulkan_context, shadow_cull_pipeline);
  }

  {
    descriptor_builder.BindStorageImage(0, main_image.image_view);
    descriptor_builder.BindStorageImage(1, mr_normal_image.image_view);
    descriptor_builder.BindStorageImage(2, depth_image.image_view);
    descriptor_builder.BindStorageImage(3, ao_image.image_view);
    descriptor_builder.BindCombinedImage(4, ao_image.image_view, sampler);
    descriptor_builder.Build(vulkan_context, VK_SHADER_STAGE_COMPUTE_BIT,
                             gbuffer_descriptor_set, gbuffer_descriptor_layout);
  }

  {
    ComputePipelineBuilder pipeline_builder{};
    pipeline_builder.SetShader(vulkan_context, "tone_map.comp.spv");
    pipeline_builder.AddDescriptorSetLayout(gbuffer_descriptor_layout);
    pipeline_builder.Build(vulkan_context, tone_map_pipeline);
  }

  {
    ComputePipelineBuilder pipeline_builder{};
    pipeline_builder.SetShader(vulkan_context, "ambient_occlusion.comp.spv");
    pipeline_builder.AddDescriptorSetLayout(gbuffer_descriptor_layout);
    pipeline_builder.AddDescriptorSetLayout(camera.descriptor_layout);
    pipeline_builder.Build(vulkan_context, ambient_occlusion_pipeline);
  }

  {
    ComputePipelineBuilder pipeline_builder{};
    pipeline_builder.SetShader(vulkan_context,
                               "upscale_ambient_occlusion.comp.spv");
    pipeline_builder.AddDescriptorSetLayout(gbuffer_descriptor_layout);
    pipeline_builder.Build(vulkan_context, upscale_ao_pipeline);
  }

  std::thread([this]() {
    SCOPED_TIMER("Scene load");
    auto gltf_data = LoadModel("Sponza.gltf");

    for (auto &mesh : gltf_data) {
      scene_manager.AddObject(
          vulkan_context, mesh,
          texture_manager.UploadMaterial(vulkan_context, descriptor_builder,
                                         mesh.material_data));
    }

    {
      /*

      descriptor_builder.BindAccelerationStructure(
          0, scene_manager.top_level_as.obj);
      descriptor_builder.BindStorageImage(1, main_image.image_view);
      descriptor_builder.BindStorageImage(2, mr_normal_image.image_view);
      descriptor_builder.BindStorageImage(3, depth_image.image_view);
      descriptor_builder.Build(
          vulkan_context,
          VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
          ray_tracing_descriptor_set, ray_tracing_descriptor_layout);

      RaytracingPipelineBuilder pipeline_builder{};
      pipeline_builder.SetShaders(vulkan_context, "reflections.rgen.spv",
                                  "reflections.rmiss.spv",
                                  "reflections.rchit.spv");
      pipeline_builder.AddDescriptorSetLayout(ray_tracing_descriptor_layout);
      pipeline_builder.AddDescriptorSetLayout(camera.descriptor_layout);
      pipeline_builder.AddDescriptorSetLayout(
          scene_manager.object_descriptor_layout);
      pipeline_builder.AddDescriptorSetLayout(
          texture_manager.descriptor_set_layout);
      pipeline_builder.AddDescriptorSetLayout(
          light_manager.light_descriptor_layout);
      pipeline_builder.AddDescriptorSetLayout(
          light_manager.shadow_descriptor_layout);
      const uint8_t max_recursion = 1;
      pipeline_builder.Build(vulkan_context, max_recursion,
      ray_tracing_pipeline);

      CreateShaderBindingTable(vulkan_context, ray_tracing_pipeline,
                               pipeline_builder.shader_groups,
                               shader_binding_table);
      */
    }
  }).detach();

  CreateRenderGraph();
}

void Engine::Run() {
  bool should_close = false;
  glm::vec3 directional_light = {-1.0, -4.0, -1.0};

  float delta_time;
  while (!glfwWindowShouldClose(window)) {
    Timer timer{};
    glfwPollEvents();
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
      glfwSetWindowShouldClose(window, true);
      should_close = true;
    }

    bool ui_layer_used = UpdateUi(window, &directional_light);

    light_manager.UpdateDirectionalLight(vulkan_context, directional_light, 0,
                                         immediate_submit);

    camera.Update(vulkan_context, immediate_submit, window, delta_time);
    light_manager.UpdateMatrices(vulkan_context, immediate_submit,
                                 camera.position);

    if (glfwGetKey(window, GLFW_KEY_T) == GLFW_PRESS) {
      fmt::println("{}", glm::to_string(camera.position));
    }

    if (glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS) {
      voxel_gi_debug = true;
    } else {
      voxel_gi_debug = false;
    }

    for (uint32_t i = 0; i < scene_svo.radiance_image_views.size(); i++) {
      if (glfwGetKey(window, GLFW_KEY_0 + i)) {
        voxel_debug_mip_level = i;
      }
    }

    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS &&
        !ui_layer_used) {
      RayCastQuery ray_query{};
      ray_query.direction = glm::vec3(0.0f, 1.0f, 0.0f);
      ray_query.position = glm::vec3(0.0f, 0.0f, 0.0f);
      ray_query.tmax = 1000.0f;
      PhysicsQueueRayCast(vulkan_context, immediate_submit, physics_context,
                          &ray_query);
    }

    std::thread([this]() {
      ImmediateSubmit::SubmitAsync(vulkan_context, [this](VkCommandBuffer cmd) {
        PhysicsCastRays(cmd, vulkan_context, physics_context,
                        scene_manager.as_descriptor_set);
      });
    }).detach();

    render_graph.Render(vulkan_context);
    if (render_graph.resize_requested == true) {
      render_graph.Resize(vulkan_context, window);
    }

    delta_time = timer.Elapsed();
    glfwSetWindowTitle(window,
                       fmt::format("{:.2f} ms", timer.ElapsedMillis()).c_str());
  }
}

void Engine::Destroy() {
  vkDeviceWaitIdle(vulkan_context.device);

  render_graph.Destroy(vulkan_context);
  light_manager.Destroy(vulkan_context);
  texture_manager.Destroy(vulkan_context);
  descriptor_builder.Destroy(vulkan_context);
  scene_manager.Destroy(vulkan_context);
  immediate_submit.Destroy(vulkan_context);
  camera.Destroy(vulkan_context);
  scene_svo.Destroy(vulkan_context);

  DestroyPhysicsContext(vulkan_context, physics_context);
  DestroyUiContext();

  vkDestroyDescriptorSetLayout(vulkan_context.device, cull_descriptor_layout,
                               nullptr);
  vkDestroyDescriptorSetLayout(vulkan_context.device, gbuffer_descriptor_layout,
                               nullptr);
  vkDestroyDescriptorSetLayout(vulkan_context.device,
                               shadow_cull_descriptor_layout, nullptr);
  vkDestroyDescriptorSetLayout(vulkan_context.device,
                               ray_tracing_descriptor_layout, nullptr);

  DestroyImageSampler(vulkan_context, sampler);

  DestroyShaderBindingTable(vulkan_context, shader_binding_table);

  DestroyBuffer(vulkan_context, draw_indirect_buffer);
  DestroyBuffer(vulkan_context, culled_draw_count_buffer);
  DestroyBuffer(vulkan_context, shadow_culled_draw_count_buffer);
  DestroyBuffer(vulkan_context, shadow_draw_indirect_buffer);

  DestroyAllocatedImage(vulkan_context, depth_image);
  DestroyAllocatedImage(vulkan_context, main_image);
  DestroyAllocatedImage(vulkan_context, mr_normal_image);
  DestroyAllocatedImage(vulkan_context, ao_image);

  DestroyPipeline(vulkan_context, main_pipeline);
  DestroyPipeline(vulkan_context, shadow_pipeline);
  DestroyPipeline(vulkan_context, cull_pipeline);
  DestroyPipeline(vulkan_context, shadow_cull_pipeline);
  DestroyPipeline(vulkan_context, ray_tracing_pipeline);
  DestroyPipeline(vulkan_context, tone_map_pipeline);
  DestroyPipeline(vulkan_context, ambient_occlusion_pipeline);
  DestroyPipeline(vulkan_context, upscale_ao_pipeline);
  DestroyPipeline(vulkan_context, depth_pipeline);

  DestroyVulkanContext(vulkan_context);

  glfwDestroyWindow(window);
  glfwTerminate();
}
