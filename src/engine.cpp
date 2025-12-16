#include "engine.h"
#include "Backend/allocated_image.h"
#include "Backend/binding_table.h"
#include "Backend/buffer.h"
#include "Backend/context.h"
#include "Backend/descriptors.h"
#include "Backend/init.h"
#include "Backend/pipeline.h"
#include "Managers/light_manager.h"
#include "Managers/scene_manager.h"
#include "Managers/texture_manager.h"
#include "Parsers/model.h"
#include "Physics/context.h"
#include "input.h"
#include "render_graph.h"
#include "timer.h"
#include <GLFW/glfw3.h>
#include <array>
#include <cstdint>
#include <fmt/base.h>
#include <fmt/format.h>
#include <imgui.h>
#include <vector>
#include <volk.h>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/string_cast.hpp>
#include <glm/gtx/transform.hpp>

void Engine::Resize(glm::vec2 new_size) {
  render_graph.Resize(window);
  camera.Resize(new_size);
  DestroyAllocatedImage(main_image);
  DestroyAllocatedImage(depth_image);
  DestroyAllocatedImage(mr_normal_image);
  DestroyAllocatedImage(ao_image);

  glm::ivec2 window_size;
  glfwGetWindowSize(window, &window_size.x, &window_size.y);

  VkExtent3D draw_image_extent = {
      static_cast<uint32_t>(new_size.x),
      static_cast<uint32_t>(new_size.y),
      1,
  };
  VkExtent3D ao_draw_image_extent = {
      draw_image_extent.width / 2,
      draw_image_extent.height / 2,
      1,
  };

  CreateAllocatedImage(draw_image_extent, VK_FORMAT_R16G16B16A16_SFLOAT,
                       VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                           VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                       main_image);
  CreateAllocatedImage(draw_image_extent, VK_FORMAT_D32_SFLOAT,
                       VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
                       depth_image);
  CreateAllocatedImage(draw_image_extent, VK_FORMAT_R8G8B8A8_UNORM,
                       VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
                       mr_normal_image);
  CreateAllocatedImage(ao_draw_image_extent, VK_FORMAT_R8_UNORM,
                       VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, ao_image);
  {
    descriptor_builder.BindStorageImage(0, main_image.image_view);
    descriptor_builder.BindStorageImage(1, mr_normal_image.image_view);
    descriptor_builder.BindStorageImage(2, depth_image.image_view);
    descriptor_builder.BindStorageImage(3, ao_image.image_view);
    descriptor_builder.BindCombinedImage(4, ao_image.image_view, sampler);
    descriptor_builder.Build(VK_SHADER_STAGE_COMPUTE_BIT, gbuffer_descriptor_set,
                             gbuffer_descriptor_layout);
  }

  editor.Resize(main_image);

  render_graph.Destroy();
  CreateRenderGraph();
}

void Engine::CreateRenderGraph() {
  RenderGraphBuilder builder{};

  {
    builder.AddPass(0, {}, [&](VkCommandBuffer cmd) {
      editor.BuildDraw(cmd, scene_manager, physics_context);

      if (scene_manager.instance_index == 0)
        return;
      {
        std::array<VkDescriptorSet, 3> ds = {
            camera.descriptor_set,
            scene_manager.instance_descriptor_set,
            scene_manager.object_descriptor_set,
        };

        main_draw_command.BuildDraw(
            cmd, ds.data(), ds.size(),
            {
                static_cast<uint32_t>(std::ceil(scene_manager.instance_index / 64.0f)),
                1,
                1,
            });
      }
      {
        std::array<VkDescriptorSet, 2> ds = {
            scene_manager.instance_descriptor_set,
            scene_manager.object_descriptor_set,
        };

        shadow_draw_command.BuildDraw(
            cmd, ds.data(), ds.size(),
            {
                static_cast<uint32_t>(std::ceil(scene_manager.instance_index / 64.0f)),
                1,
                1,
            });
      }
    });

    DependencyBuilder atmosphere_dep{};
    atmosphere_dep.AddImageDependency(skybox_image, {}, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT, {},
                                      VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, {},
                                      VK_IMAGE_LAYOUT_GENERAL);

    builder.AddPass(0, atmosphere_dep.dependency, [&](VkCommandBuffer cmd) {
      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, atmosphere_pipeline.obj);
      std::array<VkDescriptorSet, 2> ds = {
          skybox_descriptor_set,
          light_manager.light_descriptor_set,
      };

      vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, atmosphere_pipeline.layout, 0,
                              ds.size(), ds.data(), 0, nullptr);

      vkCmdDispatch(cmd, std::ceil(skybox_image.extent.width / 16.0f),
                    std::ceil(skybox_image.extent.height / 16.0f), 2);
    });

    builder.AddPass(0, {},
                    [&](VkCommandBuffer cmd) { scene_svo.BuildDrawCommands(cmd, scene_manager); });
  }

  {
    DependencyBuilder depth_pass_dep{};

    depth_pass_dep.AddImageDependency(
        depth_image, {}, VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, {},
        VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, true);

    builder.AddPass(1, depth_pass_dep.dependency, [&](VkCommandBuffer cmd) {
      VkViewport viewport = vkinit::Viewport(depth_image.extent);
      vkCmdSetViewport(cmd, 0, 1, &viewport);
      VkRect2D scissor = vkinit::Scissor(depth_image.extent);
      vkCmdSetScissor(cmd, 0, 1, &scissor);

      VkRenderingAttachmentInfo depth_att = vkinit::DepthAttachmentInfo(
          depth_image.image_view, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
          VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE);

      VkRenderingInfo render_info = vkinit::RenderingInfo(depth_image.extent, {}, &depth_att);

      vkCmdBeginRendering(cmd, &render_info);

      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, depth_pipeline.obj);

      std::array<VkDescriptorSet, 3> ds = {
          scene_manager.instance_descriptor_set,
          scene_manager.object_descriptor_set,
          camera.descriptor_set,
      };

      vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, depth_pipeline.layout, 0,
                              ds.size(), ds.data(), 0, nullptr);

      vkCmdBindIndexBuffer(cmd, scene_manager.index_buffer.buffer, 0, VK_INDEX_TYPE_UINT32);

      main_draw_command.Draw(cmd);

      vkCmdEndRendering(cmd);
    });
  }

  {
    builder.AddPass(1, {}, [&](VkCommandBuffer cmd) {
      VkViewport viewport = vkinit::Viewport(SHADOW_IMAGE_EXTENT);
      vkCmdSetViewport(cmd, 0, 1, &viewport);
      VkRect2D scissor = vkinit::Scissor(SHADOW_IMAGE_EXTENT);
      vkCmdSetScissor(cmd, 0, 1, &scissor);

      for (uint32_t i = 0; i < light_manager.matrix_index; i++) {
        AllocatedImage shadow_image = light_manager.shadow_images[i];

        TransitionImage(cmd, {}, VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, {},
                        VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT, {},
                        VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, shadow_image.image, true);

        VkRenderingAttachmentInfo depth = vkinit::DepthAttachmentInfo(
            shadow_image.image_view, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
            VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE);

        VkRenderingInfo render_info = vkinit::RenderingInfo(SHADOW_IMAGE_EXTENT, {}, &depth);

        vkCmdBeginRendering(cmd, &render_info);

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, shadow_pipeline.obj);

        vkCmdPushConstants(cmd, shadow_pipeline.layout, VK_SHADER_STAGE_VERTEX_BIT, 0,
                           sizeof(uint32_t), &i);

        std::array<VkDescriptorSet, 3> ds = {
            scene_manager.instance_descriptor_set,
            scene_manager.object_descriptor_set,
            light_manager.shadow_descriptor_set,
        };

        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, shadow_pipeline.layout, 0,
                                ds.size(), ds.data(), 0, nullptr);

        vkCmdBindIndexBuffer(cmd, scene_manager.index_buffer.buffer, 0, VK_INDEX_TYPE_UINT32);

        shadow_draw_command.Draw(cmd);

        vkCmdEndRendering(cmd);

        TransitionImage(
            cmd, VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            VK_ACCESS_2_SHADER_SAMPLED_READ_BIT, VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
            VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, shadow_image.image, true);
      }
    });
  }

  builder.AddPass(2, {}, [&](VkCommandBuffer cmd) {
    scene_svo.Build(cmd, scene_manager, texture_manager, light_manager, camera);
  });

  {
    DependencyBuilder main_pass_dep{};
    main_pass_dep.AddImageDependency(main_image, {}, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, {},
                                     VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                                     VK_IMAGE_LAYOUT_UNDEFINED,
                                     VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    main_pass_dep.AddImageDependency(mr_normal_image, {}, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                                     {}, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                                     VK_IMAGE_LAYOUT_UNDEFINED,
                                     VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

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

      VkRenderingAttachmentInfo color_att = vkinit::AttachmentInfo(
          main_image.image_view, nullptr, &clear_value, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

      VkRenderingAttachmentInfo mr_normal_att =
          vkinit::AttachmentInfo(mr_normal_image.image_view, nullptr, &clear_value,
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
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, main_pipeline.obj);

        std::array<VkDescriptorSet, 7> ds = {
            texture_manager.descriptor_set,      camera.descriptor_set,
            scene_manager.object_descriptor_set, scene_manager.instance_descriptor_set,
            light_manager.light_descriptor_set,  light_manager.shadow_descriptor_set,
            scene_svo.svo_descriptor_set,
        };

        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, main_pipeline.layout, 0,
                                ds.size(), ds.data(), 0, nullptr);

        vkCmdBindIndexBuffer(cmd, scene_manager.index_buffer.buffer, 0, VK_INDEX_TYPE_UINT32);

        main_draw_command.Draw(cmd);
      }
      vkCmdEndRendering(cmd);
    });
  }

  {
    DependencyBuilder skybox_pass{};
    skybox_pass.AddImageDependency(
        skybox_image, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
        VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    builder.AddPass(2, skybox_pass.dependency, [&](VkCommandBuffer cmd) {
      VkViewport viewport = vkinit::Viewport(main_image.extent);
      vkCmdSetViewport(cmd, 0, 1, &viewport);
      VkRect2D scissor = vkinit::Scissor(main_image.extent);
      vkCmdSetScissor(cmd, 0, 1, &scissor);

      VkRenderingAttachmentInfo color_att = vkinit::AttachmentInfo(
          main_image.image_view, nullptr, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

      std::array<VkRenderingAttachmentInfo, 1> attachments = {
          color_att,
      };

      VkRenderingAttachmentInfo depth_att = vkinit::DepthAttachmentInfo(
          depth_image.image_view, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
          VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_STORE_OP_NONE);

      VkRenderingInfo rendering_info =
          vkinit::RenderingInfo(main_image.extent, attachments, &depth_att);

      vkCmdBeginRendering(cmd, &rendering_info);

      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, skybox_pipeline.obj);

      std::array<VkDescriptorSet, 2> ds = {
          skybox_descriptor_set,
          camera.descriptor_set,
      };

      vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, skybox_pipeline.layout, 0,
                              ds.size(), ds.data(), 0, nullptr);

      vkCmdDraw(cmd, 36, 1, 0, 0);

      vkCmdEndRendering(cmd);
    });
  }

  {
    DependencyBuilder pp_pass_dep{};
    pp_pass_dep.AddImageDependency(ao_image, {}, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT, {},
                                   VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                   VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

    pp_pass_dep.AddImageDependency(
        main_image, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_2_SHADER_READ_BIT,
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);

    pp_pass_dep.AddImageDependency(
        mr_normal_image, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_2_SHADER_READ_BIT,
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);

    pp_pass_dep.AddImageDependency(
        depth_image, VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_ACCESS_2_SHADER_READ_BIT,
        VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, true);

    builder.AddPass(3, pp_pass_dep.dependency, [&](VkCommandBuffer cmd) {
      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, ambient_occlusion_pipeline.obj);

      std::array<VkDescriptorSet, 2> ds = {gbuffer_descriptor_set, camera.descriptor_set};

      vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                              ambient_occlusion_pipeline.layout, 0, ds.size(), ds.data(), 0,
                              nullptr);

      vkCmdDispatch(cmd, std::ceil(ao_image.extent.width / 16.0f),
                    std::ceil(ao_image.extent.height / 16.0f), 1);
    });
  }

  {
    DependencyBuilder ao_upscale_dep{};

    ao_upscale_dep.AddImageDependency(
        ao_image, VK_ACCESS_2_SHADER_WRITE_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    builder.AddPass(4, ao_upscale_dep.dependency, [&](VkCommandBuffer cmd) {
      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, upscale_ao_pipeline.obj);

      std::array<VkDescriptorSet, 1> ds = {
          gbuffer_descriptor_set,
      };

      vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, upscale_ao_pipeline.layout, 0,
                              ds.size(), ds.data(), 0, nullptr);

      vkCmdDispatch(cmd, std::ceil(ao_image.extent.width / 16.0f),
                    std::ceil(ao_image.extent.height / 16.0f), 1);
    });
  }

  {
    builder.AddPass(5, {}, [&](VkCommandBuffer cmd) {
      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, tone_map_pipeline.obj);

      std::array<VkDescriptorSet, 1> ds = {
          gbuffer_descriptor_set,
      };

      vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, tone_map_pipeline.layout, 0,
                              ds.size(), ds.data(), 0, nullptr);

      vkCmdDispatch(cmd, std::ceil(main_image.extent.width / 16.0f),
                    std::ceil(main_image.extent.height / 16.0f), 1);
    });
  }

  DependencyBuilder root_dep{};
  root_dep.AddImageDependency(
      depth_image, VK_ACCESS_2_SHADER_READ_BIT, VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
      VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
      VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, true);
  render_graph.Init(window);
  render_graph.render_graph = builder.render_graph;
  render_graph.root_dep = root_dep.dependency;
  render_graph.root_callback = [&](VkCommandBuffer cmd, VkImage swapchain_image,
                                   VkImageView swapchain_image_view, VkExtent2D swapchain_extent) {
    TransitionImage(cmd, VK_ACCESS_2_SHADER_READ_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                    VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                    VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_IMAGE_LAYOUT_GENERAL,
                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, main_image.image);

    editor.Draw(cmd, scene_manager, camera, main_image, depth_image);

    TransitionImage(cmd, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_2_SHADER_READ_BIT,
                    VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                    VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, main_image.image);

    editor.DrawUI(cmd, swapchain_image, swapchain_image_view,
                  {swapchain_extent.width, swapchain_extent.height, 1});

    TransitionImage(
        cmd, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, {},
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, swapchain_image);
  };
}

void Engine::Init() {
  glfwInit();
  glfwWindowHint(GLFW_CLIENT_API, GLFW_FALSE);
  VkExtent2D window_size = {1600 * 2, 900 * 2};
  window = glfwCreateWindow(window_size.width, window_size.height, "Engine", nullptr, nullptr);
  InputContext::InitCallbacks(window);

  VulkanContext::Init(window);

  descriptor_builder.Init();
  texture_manager.Init(descriptor_builder);
  scene_manager.Init(descriptor_builder);
  light_manager.Init(descriptor_builder);

  InitPhysicsContext(descriptor_builder, scene_manager.as_descriptor_layout, physics_context);

  camera.Create(descriptor_builder, glm::vec2(window_size.width, window_size.height));

  scene_svo.Create(scene_manager, light_manager, texture_manager, camera, descriptor_builder);

  light_manager.AddDirectionalLight(glm::vec3(1.0), sun_direction, 50.0);

  VkExtent3D draw_image_extent = {
      window_size.width,
      window_size.height,
      1,
  };

  CreateAllocatedImage(draw_image_extent, VK_FORMAT_R16G16B16A16_SFLOAT,
                       VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                           VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                       main_image);

  CreateAllocatedImage(draw_image_extent, VK_FORMAT_D32_SFLOAT,
                       VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
                       depth_image);

  CreateAllocatedImage(draw_image_extent, VK_FORMAT_R8G8B8A8_UNORM,
                       VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
                       mr_normal_image);

  VkExtent3D ao_draw_image_extent = {
      draw_image_extent.width / 2,
      draw_image_extent.height / 2,
      1,
  };

  CreateAllocatedImage(ao_draw_image_extent, VK_FORMAT_R8_UNORM,
                       VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, ao_image);

  CreateAllocatedImage({1024, 1024, 1}, VK_FORMAT_R16G16B16A16_SFLOAT,
                       VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, skybox_image, false,
                       true);

  {
    std::array<VkDescriptorSetLayout, 3> ds = {
        camera.descriptor_layout,
        scene_manager.instance_descriptor_layout,
        scene_manager.object_descriptor_layout,
    };

    main_draw_command.Init(descriptor_builder, "frustum_cull.comp.spv", SCENE_MAX_OBJECTS,
                           ds.data(), ds.size());
  }

  {
    std::array<VkDescriptorSetLayout, 2> ds = {
        scene_manager.instance_descriptor_layout,
        scene_manager.object_descriptor_layout,
    };

    shadow_draw_command.Init(descriptor_builder, "shadow_cull.comp.spv", SCENE_MAX_OBJECTS,
                             ds.data(), ds.size());
  }

  CreateImageSampler(sampler);

  {
    GraphicsPipelineBuilder pipeline_builder;
    pipeline_builder.SetShaders("main_pass.vert.spv", "main_pass.frag.spv");
    pipeline_builder.Default();
    pipeline_builder.AddColorAttachment(main_image.format);
    pipeline_builder.AddColorAttachment(mr_normal_image.format);
    pipeline_builder.AddDescriptorSetLayout(texture_manager.descriptor_set_layout);
    pipeline_builder.AddDescriptorSetLayout(camera.descriptor_layout);
    pipeline_builder.AddDescriptorSetLayout(scene_manager.object_descriptor_layout);
    pipeline_builder.AddDescriptorSetLayout(scene_manager.instance_descriptor_layout);
    pipeline_builder.AddDescriptorSetLayout(light_manager.light_descriptor_layout);
    pipeline_builder.AddDescriptorSetLayout(light_manager.shadow_descriptor_layout);
    pipeline_builder.AddDescriptorSetLayout(scene_svo.svo_descriptor_layout);
    pipeline_builder.Build(main_pipeline);
  }

  {
    GraphicsPipelineBuilder pipeline_builder{};
    pipeline_builder.SetShaders("shadow.vert.spv", "none.frag.spv");
    pipeline_builder.Default();
    pipeline_builder.SetCullMode(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_CLOCKWISE);
    pipeline_builder.AddDescriptorSetLayout(scene_manager.instance_descriptor_layout);
    pipeline_builder.AddDescriptorSetLayout(scene_manager.object_descriptor_layout);
    pipeline_builder.AddDescriptorSetLayout(light_manager.shadow_descriptor_layout);
    pipeline_builder.AddPushConstantRange(VK_SHADER_STAGE_VERTEX_BIT, sizeof(uint32_t));
    pipeline_builder.Build(shadow_pipeline);
  }

  {
    GraphicsPipelineBuilder pipeline_builder{};
    pipeline_builder.SetShaders("depth_pass.vert.spv", "none.frag.spv");
    pipeline_builder.Default();
    pipeline_builder.AddDescriptorSetLayout(scene_manager.instance_descriptor_layout);
    pipeline_builder.AddDescriptorSetLayout(scene_manager.object_descriptor_layout);
    pipeline_builder.AddDescriptorSetLayout(camera.descriptor_layout);
    pipeline_builder.Build(depth_pipeline);
  }

  {
    descriptor_builder.BindStorageImage(0, main_image.image_view);
    descriptor_builder.BindStorageImage(1, mr_normal_image.image_view);
    descriptor_builder.BindStorageImage(2, depth_image.image_view);
    descriptor_builder.BindStorageImage(3, ao_image.image_view);
    descriptor_builder.BindCombinedImage(4, ao_image.image_view, sampler);
    descriptor_builder.Build(VK_SHADER_STAGE_COMPUTE_BIT, gbuffer_descriptor_set,
                             gbuffer_descriptor_layout);
  }

  {
    ComputePipelineBuilder pipeline_builder{};
    pipeline_builder.SetShader("tone_map.comp.spv");
    pipeline_builder.AddDescriptorSetLayout(gbuffer_descriptor_layout);
    pipeline_builder.Build(tone_map_pipeline);
  }

  {
    ComputePipelineBuilder pipeline_builder{};
    pipeline_builder.SetShader("ambient_occlusion.comp.spv");
    pipeline_builder.AddDescriptorSetLayout(gbuffer_descriptor_layout);
    pipeline_builder.AddDescriptorSetLayout(camera.descriptor_layout);
    pipeline_builder.Build(ambient_occlusion_pipeline);
  }

  {
    ComputePipelineBuilder pipeline_builder{};
    pipeline_builder.SetShader("upscale_ambient_occlusion.comp.spv");
    pipeline_builder.AddDescriptorSetLayout(gbuffer_descriptor_layout);
    pipeline_builder.Build(upscale_ao_pipeline);
  }

  {
    descriptor_builder.BindStorageImage(0, skybox_image.image_view);
    descriptor_builder.BindCombinedImage(1, skybox_image.image_view, texture_manager.sampler);
    descriptor_builder.Build(VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                             skybox_descriptor_set, skybox_descriptor_layout);
  }

  {
    ComputePipelineBuilder pipeline_builder{};
    pipeline_builder.SetShader("skybox.comp.spv");
    pipeline_builder.AddDescriptorSetLayout(skybox_descriptor_layout);
    pipeline_builder.AddDescriptorSetLayout(light_manager.light_descriptor_layout);
    pipeline_builder.Build(atmosphere_pipeline);
  }

  {
    GraphicsPipelineBuilder pipeline_builder{};
    pipeline_builder.SetShaders("skybox.vert.spv", "skybox.frag.spv");
    pipeline_builder.Default();
    pipeline_builder.AddColorAttachment(main_image.format);
    pipeline_builder.AddDescriptorSetLayout(skybox_descriptor_layout);
    pipeline_builder.AddDescriptorSetLayout(camera.descriptor_layout);
    pipeline_builder.Build(skybox_pipeline);
  }

  CreateRenderGraph();

  editor.Init(descriptor_builder, scene_manager, camera, window, main_image, depth_image.format,
              render_graph.swapchain.format);
}

void Engine::Run() {
  InputContext::dropped_file_queue.push_back(
      std::filesystem::path("C:/Users/ethan/Developer/Agartha/assets/models/Sponza.gltf"));

  while (!glfwWindowShouldClose(window)) {
    Timer timer{};

    InputContext::Update(window);

    if (!InputContext::dropped_file_queue.empty()) {
      for (uint32_t i = 0; i < InputContext::dropped_file_queue.size(); i++) {
        std::filesystem::path file_path = InputContext::dropped_file_queue[i];

        fmt::println("{}", file_path.string());

        std::thread([this, file_path]() {
          SCOPED_TIMER("model load");
          std::vector<SceneNodeData> scene_node_data = ParseModel(file_path.string());

          for (auto node : scene_node_data) {
            scene_manager.AddSceneNode(node, texture_manager);
          }
        }).detach();
      }
      InputContext::dropped_file_queue.clear();
    }

    camera.Update(window, delta_time);

    if (InputContext::GetInputPressed(Input::ESCAPE)) {
      glfwSetWindowShouldClose(window, true);
    }

    editor.Update(physics_context, scene_manager, texture_manager, camera);

    light_manager.UpdateMatrices(camera.position);

    UpdatePhysicsContext(scene_manager.as_descriptor_set, physics_context);

    render_graph.Render();

    if (editor.resize_requested == true) {
      Resize(editor.scene_window_size);
    }

    delta_time = timer.Elapsed();
    glfwSetWindowTitle(window, fmt::format("{:.2f} ms", timer.ElapsedMillis()).c_str());
  }
}

void Engine::Destroy() {
  vkDeviceWaitIdle(VulkanContext::device);

  editor.Destroy();

  render_graph.Destroy();
  light_manager.Destroy();
  texture_manager.Destroy();
  descriptor_builder.Destroy();
  scene_manager.Destroy();
  camera.Destroy();
  scene_svo.Destroy();

  DestroyPhysicsContext(physics_context);

  main_draw_command.Destroy();
  shadow_draw_command.Destroy();

  vkDestroyDescriptorSetLayout(VulkanContext::device, gbuffer_descriptor_layout, nullptr);
  vkDestroyDescriptorSetLayout(VulkanContext::device, ray_tracing_descriptor_layout, nullptr);
  vkDestroyDescriptorSetLayout(VulkanContext::device, skybox_descriptor_layout, nullptr);

  DestroyImageSampler(sampler);

  DestroyShaderBindingTable(shader_binding_table);

  DestroyAllocatedImage(depth_image);
  DestroyAllocatedImage(main_image);
  DestroyAllocatedImage(mr_normal_image);
  DestroyAllocatedImage(ao_image);
  DestroyAllocatedImage(skybox_image);

  DestroyPipeline(main_pipeline);
  DestroyPipeline(shadow_pipeline);
  DestroyPipeline(cull_pipeline);
  DestroyPipeline(shadow_cull_pipeline);
  DestroyPipeline(ray_tracing_pipeline);
  DestroyPipeline(tone_map_pipeline);
  DestroyPipeline(ambient_occlusion_pipeline);
  DestroyPipeline(upscale_ao_pipeline);
  DestroyPipeline(depth_pipeline);
  DestroyPipeline(atmosphere_pipeline);
  DestroyPipeline(skybox_pipeline);

  VulkanContext::Destroy();

  glfwDestroyWindow(window);
  glfwTerminate();
}
