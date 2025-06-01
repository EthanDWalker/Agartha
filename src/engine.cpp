#include "engine.h"
#include "Backend/allocated_image.h"
#include "Backend/buffer.h"
#include "Backend/context.h"
#include "Backend/descriptors.h"
#include "Backend/init.h"
#include "Backend/pipeline.h"
#include "Loaders/model.h"
#include "Managers/scene_manager.h"
#include "Managers/texture_manager.h"
#include "fmt/format.h"
#include "render_graph.h"
#include "timer.h"
#include "types.h"
#include <GLFW/glfw3.h>
#include <array>
#include <cstdint>
#include <cstring>
#include <fmt/base.h>
#include <vector>
#include <vulkan/vulkan_core.h>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>
#include <glm/gtx/transform.hpp>

void Engine::CreateRenderGraph() {
  RenderGraphBuilder builder{};

  {
    DependencyBuilder shadow_pass_dep{};
    shadow_pass_dep.AddImageTransition(VK_IMAGE_LAYOUT_UNDEFINED,
                                       VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                                       shadow_image);

    builder.AddPass(0, shadow_pass_dep.dependency, [&](VkCommandBuffer cmd) {
      /*
        VkViewport viewport = vkinit::Viewport(shadow_image.extent);
        vkCmdSetViewport(cmd, 0, 1, &viewport);
        VkRect2D scissor = vkinit::Scissor(shadow_image.extent);
        vkCmdSetScissor(cmd, 0, 1, &scissor);

        VkRenderingAttachmentInfo depth_att = vkinit::DepthAttachmentInfo(
            shadow_image.image_view, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

        VkRenderingInfo rendering_info =
            vkinit::RenderingInfo(shadow_image.extent, nullptr, &depth_att);

        vkCmdBeginRendering(cmd, &rendering_info);

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          shadow_pipeline.obj);

        VkDescriptorSet ds[] = {shadow_descriptor_set,
                                instance_manager.descriptor_set};

        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                shadow_pipeline.layout, 0, 2, ds, 0, nullptr);

        for (auto &object : scene) {
          DrawObject(cmd, shadow_pipeline, object);
        }

        vkCmdEndRendering(cmd);
      */
    });

    /*
    builder.AddPass(0, {}, [&](VkCommandBuffer cmd) {
      VkViewport viewport = vkinit::Viewport(draw_image.extent);
      vkCmdSetViewport(cmd, 0, 1, &viewport);
      VkRect2D scissor = vkinit::Scissor(draw_image.extent);
      vkCmdSetScissor(cmd, 0, 1, &scissor);

      VkRenderingAttachmentInfo color_att =
          vkinit::AttachmentInfo(draw_image.image_view, nullptr, nullptr,
                                 VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

      VkRenderingAttachmentInfo depth_att = vkinit::DepthAttachmentInfo(
          depth_image.image_view, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

      VkRenderingInfo rendering_info =
          vkinit::RenderingInfo(draw_image.extent, &color_att, &depth_att);

      vkCmdBeginRendering(cmd, &rendering_info);

      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                        skybox_pipeline.obj);

      VkDescriptorSet ds[] = {skybox.descriptor_set, camera.descriptor_set};

      vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                              skybox_pipeline.layout, 0, 2, ds, 0, nullptr);

      DrawObject(cmd, skybox_pipeline, cube_obj);

      vkCmdEndRendering(cmd);
    });
    */

    DependencyBuilder cull_pass_dep{};
    cull_pass_dep.AddDependency(camera.ubo, VK_ACCESS_2_TRANSFER_WRITE_BIT,
                                VK_ACCESS_2_SHADER_READ_BIT,
                                VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT,
                                VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);

    builder.AddPass(0, cull_pass_dep.dependency, [&](VkCommandBuffer cmd) {
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
  }

  {
    DependencyBuilder main_pass_dep{};
    main_pass_dep.AddImageTransition(VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                                     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                     shadow_image);
    main_pass_dep.AddImageTransition(VK_IMAGE_LAYOUT_UNDEFINED,
                                     VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                                     depth_image);
    main_pass_dep.AddImageTransition(VK_IMAGE_LAYOUT_UNDEFINED,
                                     VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                     draw_image);

    main_pass_dep.AddDependency(
        culled_draw_count_buffer, VK_ACCESS_2_SHADER_WRITE_BIT,
        VK_ACCESS_2_HOST_READ_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_2_HOST_BIT);
    main_pass_dep.AddDependency(draw_indirect_buffer,
                                VK_ACCESS_2_SHADER_WRITE_BIT,
                                VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT,
                                VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT);
    main_pass_dep.AddDependency(
        visible_instance_buffer, VK_ACCESS_2_SHADER_WRITE_BIT,
        VK_ACCESS_2_SHADER_READ_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT);

    builder.AddPass(1, main_pass_dep.dependency, [&](VkCommandBuffer cmd) {
      VkViewport viewport = vkinit::Viewport(draw_image.extent);
      vkCmdSetViewport(cmd, 0, 1, &viewport);
      VkRect2D scissor = vkinit::Scissor(draw_image.extent);
      vkCmdSetScissor(cmd, 0, 1, &scissor);

      VkClearColorValue clear_color_value{};
      clear_color_value = {0.0f, 0.0f, 0.0f, 0.0f};

      VkClearValue clear_value{};
      clear_value.color = clear_color_value;

      VkRenderingAttachmentInfo color_att =
          vkinit::AttachmentInfo(draw_image.image_view, nullptr, &clear_value,
                                 VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

      VkRenderingAttachmentInfo depth_att = vkinit::DepthAttachmentInfo(
          depth_image.image_view, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

      VkRenderingInfo rendering_info =
          vkinit::RenderingInfo(draw_image.extent, &color_att, &depth_att);

      vkCmdBeginRendering(cmd, &rendering_info);

      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                        mesh_pipeline.obj);

      uint32_t draw_count = 0;
      memcpy(&draw_count, culled_draw_count_buffer.info.pMappedData,
             sizeof(uint32_t));

      if (draw_count > scene_manager.instance_index) {
        vkCmdEndRendering(cmd);
        return;
      }

      std::array<VkDescriptorSet, 5> ds = {
          mesh_descriptor_set,
          texture_manager.descriptor_set,
          camera.descriptor_set,
          scene_manager.object_descriptor_set,
          scene_manager.instance_descriptor_set,
      };

      vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                              mesh_pipeline.layout, 0, ds.size(), ds.data(), 0,
                              nullptr);

      vkCmdBindIndexBuffer(cmd, scene_manager.index_buffer.buffer, 0,
                           VK_INDEX_TYPE_UINT32);

      vkCmdDrawIndexedIndirect(
          cmd, draw_indirect_buffer.buffer, 0, draw_count,
          static_cast<uint32_t>(sizeof(VkDrawIndexedIndirectCommand)));

      vkCmdEndRendering(cmd);
    });
  }

  render_graph.Init(context, window);
  render_graph.render_graph = builder.render_graph;
  render_graph.root_callback = [&](VkCommandBuffer cmd, VkImage swapchain_image,
                                   VkExtent2D swapchain_extent) {
    TransitionImage(cmd, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, draw_image.image);

    TransitionImage(cmd, VK_IMAGE_LAYOUT_UNDEFINED,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, swapchain_image);

    CopyImageToImage(cmd, draw_image.image, swapchain_image,
                     {draw_image.extent.width, draw_image.extent.height},
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
  texture_manager.Init(context, descriptor_builder);
  scene_manager.Init(context, descriptor_builder);
  thread_pool.Create(context);
  camera.Create(context, descriptor_builder);

  CreateAllocatedImage(context, {1024 * 4, 1024 * 4, 1}, VK_FORMAT_D32_SFLOAT,
                       VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
                           VK_IMAGE_USAGE_SAMPLED_BIT |
                           VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                       shadow_image);

  VkExtent3D draw_image_extent = {
      1600,
      900,
      1,
  };

  CreateAllocatedImage(
      context, draw_image_extent, VK_FORMAT_R16G16B16A16_SFLOAT,
      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
          VK_IMAGE_USAGE_STORAGE_BIT,
      draw_image);

  CreateAllocatedImage(context, draw_image_extent, VK_FORMAT_D32_SFLOAT,
                       VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                       depth_image);

  CreateImageSampler(context, sampler);

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

  CreateSkybox(context, immediate_submit, descriptor_builder, texture_manager,
               "sunset", skybox);

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

  {
    descriptor_builder.Reset();
    descriptor_builder.BindUniformBuffer(0, point_light_buffer.buffer);
    descriptor_builder.BindUniformBuffer(1, directional_light_buffer.buffer);
    descriptor_builder.BindSampler(2, sampler);
    descriptor_builder.BindCombinedImage(3, shadow_image.image_view,
                                         shadow_sampler);
    descriptor_builder.BindUniformBuffer(4, light_matrix_buffer.buffer);
    descriptor_builder.BindStorageBuffer(5, visible_instance_buffer.buffer);
    descriptor_builder.Build(
        context, VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT,
        mesh_descriptor_set, mesh_descriptor_layout);

    GraphicsPipelineBuilder pipeline_builder;
    pipeline_builder.SetShaders(context, "mesh.vert.spv", "mesh.frag.spv");
    pipeline_builder.Default();
    pipeline_builder.AddDescriptorSetLayout(mesh_descriptor_layout);
    pipeline_builder.AddDescriptorSetLayout(
        texture_manager.descriptor_set_layout);
    pipeline_builder.AddDescriptorSetLayout(camera.descriptor_layout);
    pipeline_builder.AddDescriptorSetLayout(
        scene_manager.object_descriptor_layout);
    pipeline_builder.AddDescriptorSetLayout(
        scene_manager.instance_descriptor_layout);
    pipeline_builder.Build(context, mesh_pipeline);
  }

  /*{
    GraphicsPipelineBuilder pipeline_builder;
    pipeline_builder.SetShaders(context, "skybox.vert.spv", "skybox.frag.spv");
    pipeline_builder.Default();
    pipeline_builder.SetCullMode(VK_CULL_MODE_BACK_BIT,
                                 VK_FRONT_FACE_CLOCKWISE);
    pipeline_builder.AddDescriptorSetLayout(skybox.descriptor_layout);
    pipeline_builder.AddDescriptorSetLayout(camera.descriptor_layout);
    pipeline_builder.Build(context, skybox_pipeline);
  }*/

  /*{
    descriptor_builder.Reset();
    descriptor_builder.BindUniformBuffer(0, light_matrix_buffer.buffer);
    descriptor_builder.Build(context, VK_SHADER_STAGE_VERTEX_BIT,
                             shadow_descriptor_set,
                             shadow_descriptor_set_layout);
    GraphicsPipelineBuilder pipeline_builder;
    pipeline_builder.Default();
    pipeline_builder.SetShaders(context, "shadow.vert.spv", "shadow.frag.spv");
    pipeline_builder.SetDepthFormat(shadow_image.format);
    pipeline_builder.AddDescriptorSetLayout(shadow_descriptor_set_layout);
    //
  pipeline_builder.AddDescriptorSetLayout(instance_manager.descriptor_layout);
    pipeline_builder.Build(context, shadow_pipeline);
  }*/

  {
    descriptor_builder.Reset();
    descriptor_builder.BindStorageBuffer(0, draw_indirect_buffer.buffer);
    descriptor_builder.BindStorageBuffer(1, culled_draw_count_buffer.buffer);
    descriptor_builder.BindStorageBuffer(2, visible_instance_buffer.buffer);
    descriptor_builder.Build(context, VK_SHADER_STAGE_COMPUTE_BIT,
                             cull_descriptor_set, cull_descriptor_set_layout);
    ComputePipelineBuilder pipeline_builder{};
    pipeline_builder.SetShader(context, "frustum_cull.comp.spv");
    pipeline_builder.AddDescriptorSetLayout(cull_descriptor_set_layout);
    pipeline_builder.AddDescriptorSetLayout(camera.descriptor_layout);
    pipeline_builder.AddDescriptorSetLayout(
        scene_manager.instance_descriptor_layout);
    pipeline_builder.AddDescriptorSetLayout(
        scene_manager.object_descriptor_layout);
    pipeline_builder.Build(context, cull_pipeline);
  }

  std::vector<MeshData> gltf_data = LoadModel("Sponza.gltf");

  {
    SCOPED_TIMER("Scene load");
    for (auto &mesh_data : gltf_data) {
      Material object_material =
          texture_manager.GetMaterial(mesh_data.material_data);
      scene_manager.AddObject(context, immediate_submit, mesh_data,
                              object_material);
    }
  }

  camera.position = {2, 2, 2};
  camera.Update(context, immediate_submit, window, 0.001f);

  CreateRenderGraph();
}

void Engine::Run() {
  const float distance = 200.0f;
  const float scene_extent = 85.0f;
  const float far_plane = 240.0f;
  const float near_plane = 0.1f;

  {
    glm::vec3 light_dir = normalize(directional_light.direction);
    glm::vec3 light_pos = glm::zero<glm::vec3>() - light_dir * distance;
    glm::vec3 up = glm::vec3(0.0f, -1.0f, 0.0f);

    glm::mat4 light_view = glm::lookAt(light_pos, glm::zero<glm::vec3>(), up);

    glm::mat4 light_projection =
        glm::ortho(-scene_extent, scene_extent, -scene_extent, scene_extent,
                   near_plane, far_plane);

    glm::mat4 light_matrix = light_projection * light_view;

    UpdateBuffer(context, immediate_submit, &light_matrix, sizeof(glm::mat4), 0,
                 light_matrix_buffer);
  }

  while (!glfwWindowShouldClose(window)) {
    Timer timer{};
    glfwPollEvents();
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
      glfwSetWindowShouldClose(window, true);
    }

    camera.Update(context, immediate_submit, window, 0.001f);

    render_graph.Render(context);
    if (render_graph.resize_requested == true) {
      render_graph.Resize(context, window);
    }

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
  scene_manager.Destroy(context);
  thread_pool.Destroy(context);
  immediate_submit.Destroy(context);
  camera.Destroy(context);

  vkDestroyDescriptorSetLayout(context.device, mesh_descriptor_layout, nullptr);
  vkDestroyDescriptorSetLayout(context.device, shadow_descriptor_set_layout,
                               nullptr);
  vkDestroyDescriptorSetLayout(context.device, cull_descriptor_set_layout,
                               nullptr);

  DestroyImageSampler(context, sampler);
  DestroyImageSampler(context, shadow_sampler);

  DestroySkybox(context, skybox);

  DestroyBuffer(context, point_light_buffer);
  DestroyBuffer(context, directional_light_buffer);
  DestroyBuffer(context, light_matrix_buffer);
  DestroyBuffer(context, draw_indirect_buffer);
  DestroyBuffer(context, culled_draw_count_buffer);
  DestroyBuffer(context, visible_instance_buffer);

  DestroyAllocatedImage(context, shadow_image);
  DestroyAllocatedImage(context, draw_image);
  DestroyAllocatedImage(context, depth_image);

  DestroyPipeline(context, mesh_pipeline);
  DestroyPipeline(context, skybox_pipeline);
  DestroyPipeline(context, shadow_pipeline);
  DestroyPipeline(context, cull_pipeline);

  DestroyVulkanContext(context);

  glfwDestroyWindow(window);
  glfwTerminate();
}
