#include "engine.h"
#include "Backend/allocated_image.h"
#include "Backend/buffer.h"
#include "Backend/context.h"
#include "Backend/descriptors.h"
#include "Backend/frame_data.h"
#include "Backend/init.h"
#include "Backend/pipeline.h"
#include "Backend/swapchain.h"
#include "Backend/util.h"
#include "Loaders/model.h"
#include "Primitives/cube.h"
#include "fmt/base.h"
#include "object.h"
#include "texture_manager.h"
#include "timer.h"
#include "types.h"
#include <GLFW/glfw3.h>
#include <cstdint>
#include <limits>
#include <vector>
#include <vulkan/vulkan_core.h>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>
#include <glm/gtx/transform.hpp>

void Engine::Init() {
  glfwInit();
  glfwWindowHint(GLFW_CLIENT_API, GLFW_FALSE);
  window = glfwCreateWindow(1600, 900, "Engine", nullptr, nullptr);
  InitVulkanContext(window, DEBUG, context);
  CreateVulkanSwapchain(context, 1600, 900, swapchain);
  immediate_submit.Create(context);
  descriptor_builder.Init(context);
  texture_manager.Init(context, descriptor_builder);
  camera.Create(context, descriptor_builder);

  for (FrameData &frame : frame_data) {
    CreateFrameData(context, frame);
  }

  VkExtent3D draw_image_extent_3d = {
      swapchain.extent.width,
      swapchain.extent.height,
      1,
  };

  CreateAllocatedImage(context, draw_image_extent_3d,
                       VK_FORMAT_R16G16B16A16_SFLOAT,
                       VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                           VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT,
                       msaa_draw_image, 1, false, VK_SAMPLE_COUNT_4_BIT);

  CreateAllocatedImage(
      context, draw_image_extent_3d, VK_FORMAT_R16G16B16A16_SFLOAT,
      VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT |
          VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
      draw_image);

  CreateAllocatedImage(context, draw_image_extent_3d, VK_FORMAT_D32_SFLOAT,
                       VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, depth_image,
                       1, false, VK_SAMPLE_COUNT_4_BIT);

  CreateAllocatedImage(context, {1024 * 4, 1024 * 4, 1}, VK_FORMAT_D32_SFLOAT,
                       VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
                           VK_IMAGE_USAGE_SAMPLED_BIT |
                           VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                       shadow_image);

  CreateImageSampler(context, sampler);

  VkSamplerCreateInfo shadow_sampler_ci{};
  shadow_sampler_ci.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  shadow_sampler_ci.magFilter = VK_FILTER_LINEAR;
  shadow_sampler_ci.minFilter = VK_FILTER_LINEAR;
  shadow_sampler_ci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
  shadow_sampler_ci.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  shadow_sampler_ci.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  shadow_sampler_ci.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  shadow_sampler_ci.compareEnable = VK_TRUE; // Using manual PCF in shader
  shadow_sampler_ci.compareOp =
      VK_COMPARE_OP_GREATER_OR_EQUAL; // Only used if compareEnable = true
  shadow_sampler_ci.anisotropyEnable =
      VK_FALSE; // No need for anisotropy in shadow maps
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

  {
    descriptor_builder.Reset();
    descriptor_builder.BindBuffer(0, point_light_buffer.buffer);
    descriptor_builder.BindBuffer(1, directional_light_buffer.buffer);
    descriptor_builder.BindBuffer(2, camera.ubo.buffer);
    descriptor_builder.BindSampler(3, sampler);
    descriptor_builder.BindImage(4, skybox.prefilter.image_view);
    descriptor_builder.BindImage(5, skybox.irradiance.image_view);
    descriptor_builder.BindImage(6, skybox.brdf.image_view);
    descriptor_builder.BindCombinedImage(7, shadow_image.image_view,
                                         shadow_sampler);
    descriptor_builder.BindBuffer(8, light_matrix_buffer.buffer);
    descriptor_builder.Build(
        context, VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT,
        descriptor_set, descriptor_layout);

    GraphicsPipelineBuilder pipeline_builder;
    pipeline_builder.SetShaders(context, "mesh.vert.spv", "mesh.frag.spv");
    pipeline_builder.Default();
    pipeline_builder.SetDepthFormat(depth_image.format);
    pipeline_builder.AddPushConstantRange(VK_SHADER_STAGE_VERTEX_BIT |
                                              VK_SHADER_STAGE_FRAGMENT_BIT,
                                          sizeof(ObjectPushConstantData));
    pipeline_builder.AddDescriptorSetLayout(descriptor_layout);
    pipeline_builder.AddDescriptorSetLayout(
        texture_manager.descriptor_set_layout);
    pipeline_builder.Build(context, mesh_pipeline);
  }

  {
    descriptor_builder.Reset();
    descriptor_builder.BindCombinedImage(0, skybox.image.image_view, sampler);
    descriptor_builder.BindBuffer(1, camera.ubo.buffer);
    descriptor_builder.Build(
        context, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        skybox_descriptor_set, skybox_descriptor_layout);

    GraphicsPipelineBuilder pipeline_builder;
    pipeline_builder.SetShaders(context, "skybox.vert.spv", "skybox.frag.spv");
    pipeline_builder.Default();
    pipeline_builder.SetCullMode(VK_CULL_MODE_BACK_BIT,
                                 VK_FRONT_FACE_CLOCKWISE);
    pipeline_builder.SetDepthFormat(depth_image.format);
    pipeline_builder.AddPushConstantRange(VK_SHADER_STAGE_VERTEX_BIT |
                                              VK_SHADER_STAGE_FRAGMENT_BIT,
                                          sizeof(ObjectPushConstantData));
    pipeline_builder.AddDescriptorSetLayout(skybox_descriptor_layout);
    pipeline_builder.Build(context, skybox_pipeline);
  }

  {
    descriptor_builder.Reset();
    descriptor_builder.BindBuffer(0, light_matrix_buffer.buffer);
    descriptor_builder.Build(context, VK_SHADER_STAGE_VERTEX_BIT,
                             shadow_descriptor_set,
                             shadow_descriptor_set_layout);
    GraphicsPipelineBuilder pipeline_builder;
    pipeline_builder.Default();
    pipeline_builder.SetCullMode(VK_CULL_MODE_FRONT_BIT,
                                 VK_FRONT_FACE_CLOCKWISE);
    pipeline_builder.SetNoMultisampling();
    pipeline_builder.SetShaders(context, "shadow.vert.spv", "shadow.frag.spv");
    pipeline_builder.SetDepthFormat(shadow_image.format);
    pipeline_builder.AddPushConstantRange(VK_SHADER_STAGE_VERTEX_BIT |
                                              VK_SHADER_STAGE_FRAGMENT_BIT,
                                          sizeof(ObjectPushConstantData));
    pipeline_builder.AddDescriptorSetLayout(shadow_descriptor_set_layout);
    pipeline_builder.Build(context, shadow_pipeline);
  }

  {
    GraphicsPipelineBuilder pipeline_builder{};
    pipeline_builder.Default();
    pipeline_builder.SetInputTopology(VK_PRIMITIVE_TOPOLOGY_LINE_LIST);
    pipeline_builder.SetShaders(context, "debug_aabb.vert.spv",
                                "debug_aabb.frag.spv", "debug_aabb.geom.spv");
    pipeline_builder.AddDescriptorSetLayout(camera.descriptor_layout);
    pipeline_builder.AddPushConstantRange(VK_SHADER_STAGE_VERTEX_BIT,
                                          sizeof(VkDeviceAddress));
    pipeline_builder.SetDepthFormat(depth_image.format);
    pipeline_builder.Build(context, aabb_pipeline);
  }

  std::vector<MeshData> gltf_data = LoadModel("Sponza.gltf");

  MeshData cube_data = {
      .vertices = cube_vertices,
      .indices = cube_indices,
  };

  scene.reserve(gltf_data.size());

  std::vector<AABB> aabbs;
  aabbs.reserve(gltf_data.size());

  {
    uint32_t index;
    for (auto &mesh_data : gltf_data) {
      Object object;
      CreateObjectMaterial(context, immediate_submit, descriptor_builder,
                           texture_manager, mesh_data, object);

      for (auto &instance : mesh_data.instances) {
        AddObjectInstanceMatrix(context, immediate_submit, instance, object);
        aabbs.push_back({
            .min = glm::vec3(instance * glm::vec4(mesh_data.aabb.min, 1.0)),
            .max = glm::vec3(instance * glm::vec4(mesh_data.aabb.max, 1.0)),
        });
      }

      scene.push_back(object);
      index++;
    }
  }
  fmt::println("{}", aabbs.size());

  CreateBufferData(context, immediate_submit, aabbs.data(),
                   aabbs.size() * sizeof(AABB),
                   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                       VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                   aabb_buffer);

  CreateObject(context, immediate_submit, descriptor_builder, cube_data,
               cube_obj);

  AddObjectInstanceMatrix(context, immediate_submit, glm::mat4(1.0f), cube_obj);

  camera.position = {2, 2, 2};
  camera.Update(context, immediate_submit, window, 0.001f);
}

void Engine::Run() {
  uint8_t frame_index = 0;

  float distance = 200.0f;
  float scene_extent = 85.0f;
  float far_plane = 240.0f;
  float near_plane = 0.1f;

  {
    glm::vec3 light_dir = normalize(glm::vec3(-1.0f, -3.0f, -1.0f));
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

  bool debug_aabb = false;

  while (!glfwWindowShouldClose(window)) {
    Timer timer{};
    glfwPollEvents();
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
      glfwSetWindowShouldClose(window, true);
    }
    if (glfwGetKey(window, GLFW_KEY_T) == GLFW_PRESS) {
      debug_aabb = true;
    } else if (glfwGetKey(window, GLFW_KEY_T) != GLFW_PRESS) {
      debug_aabb = false;
    }

    camera.Update(context, immediate_submit, window, 0.001f);

    VK_CHECK(vkWaitForFences(context.device, 1,
                             &frame_data[frame_index].render_fence, VK_TRUE,
                             std::numeric_limits<uint32_t>::max()));
    VK_CHECK(vkResetFences(context.device, 1,
                           &frame_data[frame_index].render_fence));

    uint32_t swapchain_image_index;
    {
      VkResult e =
          vkAcquireNextImageKHR(context.device, swapchain.obj, 1000000000,
                                frame_data[frame_index].swapchain_semaphore,
                                nullptr, &swapchain_image_index);
      if (e == VK_ERROR_OUT_OF_DATE_KHR) {
        int32_t width, height;
        glfwGetWindowSize(window, &width, &height);
        vkDeviceWaitIdle(context.device);
        DestroyVulkanSwapchain(context, swapchain);
        CreateVulkanSwapchain(context, width, height, swapchain);
      }
    }

    VkCommandBuffer cmd = frame_data[frame_index].command_buffer;

    VK_CHECK(vkResetCommandBuffer(cmd, 0));

    VkCommandBufferBeginInfo cmd_begin_info = vkinit::CommandBufferBeginInfo(
        VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    VK_CHECK(vkBeginCommandBuffer(cmd, &cmd_begin_info));

    TransitionImage(cmd, VK_IMAGE_LAYOUT_UNDEFINED,
                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, draw_image.image);

    TransitionImage(cmd, VK_IMAGE_LAYOUT_UNDEFINED,
                    VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                    depth_image.image);

    TransitionImage(cmd, VK_IMAGE_LAYOUT_UNDEFINED,
                    VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                    shadow_image.image);

    VkClearColorValue clear_color_value{};
    clear_color_value = {0.0f, 0.0f, 0.0f, 0.0f};

    VkClearValue clear_value{};
    clear_value.color = clear_color_value;

    {
      Timer timer{};
      VkRenderingAttachmentInfo shadow_attachment_info =
          vkinit::DepthAttachmentInfo(shadow_image.image_view,
                                      VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

      VkRenderingInfo rendering_info = vkinit::RenderingInfo(
          {shadow_image.extent.width, shadow_image.extent.height}, nullptr,
          &shadow_attachment_info);

      vkCmdBeginRendering(cmd, &rendering_info);

      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                        shadow_pipeline.obj);

      vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                              shadow_pipeline.layout, 0, 1,
                              &shadow_descriptor_set, 0, nullptr);

      VkViewport viewport = {};
      viewport.x = 0;
      viewport.y = 0;
      viewport.width = shadow_image.extent.width;
      viewport.height = shadow_image.extent.height;
      viewport.minDepth = 1.0f;
      viewport.maxDepth = 0.0f;

      vkCmdSetViewport(cmd, 0, 1, &viewport);

      VkRect2D scissor = {};
      scissor.offset.x = 0;
      scissor.offset.y = 0;
      scissor.extent.width = shadow_image.extent.width;
      scissor.extent.height = shadow_image.extent.height;

      vkCmdSetScissor(cmd, 0, 1, &scissor);

      for (auto &object : scene) {
        DrawObject(cmd, shadow_pipeline, object);
      }

      vkCmdEndRendering(cmd);
    }

    VkRenderingAttachmentInfo attachment_info = vkinit::AttachmentInfo(
        msaa_draw_image.image_view, draw_image.image_view, &clear_value,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    VkRenderingAttachmentInfo depth_attachment_info =
        vkinit::DepthAttachmentInfo(depth_image.image_view,
                                    VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

    VkRenderingInfo rendering_info = vkinit::RenderingInfo(
        {draw_image.extent.width, draw_image.extent.height}, &attachment_info,
        &depth_attachment_info);

    TransitionImage(cmd, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    shadow_image.image);

    vkCmdBeginRendering(cmd, &rendering_info);

    {
      Timer timer{};
      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                        skybox_pipeline.obj);

      VkViewport viewport = {};
      viewport.x = 0;
      viewport.y = 0;
      viewport.width = draw_image.extent.width;
      viewport.height = draw_image.extent.height;
      viewport.minDepth = 1.0f;
      viewport.maxDepth = 0.0f;

      vkCmdSetViewport(cmd, 0, 1, &viewport);

      VkRect2D scissor = {};
      scissor.offset.x = 0;
      scissor.offset.y = 0;
      scissor.extent.width = draw_image.extent.width;
      scissor.extent.height = draw_image.extent.height;

      vkCmdSetScissor(cmd, 0, 1, &scissor);

      vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                              skybox_pipeline.layout, 0, 1,
                              &skybox_descriptor_set, 0, nullptr);

      DrawObject(cmd, skybox_pipeline, cube_obj);
    }

    if (debug_aabb) {
      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                        aabb_pipeline.obj);
      vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                              aabb_pipeline.layout, 0, 1,
                              &camera.descriptor_set, 0, nullptr);

      VkBufferDeviceAddressInfo device_address_info{};
      device_address_info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
      device_address_info.buffer = aabb_buffer.buffer;

      VkDeviceAddress address =
          vkGetBufferDeviceAddress(context.device, &device_address_info);

      vkCmdPushConstants(cmd, aabb_pipeline.layout, VK_SHADER_STAGE_VERTEX_BIT,
                         0, sizeof(VkDeviceAddress), &address);

      vkCmdDraw(cmd, 50 * 2, 1, 0, 0);
    }

    {
      Timer timer{};
      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                        mesh_pipeline.obj);

      vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                              mesh_pipeline.layout, 1, 1,
                              &texture_manager.descriptor_set, 0, nullptr);

      vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                              mesh_pipeline.layout, 0, 1, &descriptor_set, 0,
                              nullptr);

      for (auto &object : scene) {
        DrawObject(cmd, mesh_pipeline, object);
      }
    }

    Timer other_timer{};
    vkCmdEndRendering(cmd);

    TransitionImage(cmd, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, draw_image.image);

    TransitionImage(cmd, VK_IMAGE_LAYOUT_UNDEFINED,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    swapchain.images[swapchain_image_index]);

    CopyImageToImage(
        cmd, draw_image.image, swapchain.images[swapchain_image_index],
        {draw_image.extent.width, draw_image.extent.height}, swapchain.extent);

    TransitionImage(cmd, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                    swapchain.images[swapchain_image_index]);

    VK_CHECK(vkEndCommandBuffer(cmd));

    VkCommandBufferSubmitInfo cmd_info = vkinit::CommandBufferSubmitInfo(cmd);

    VkSemaphoreSubmitInfo wait_semaphore_info = vkinit::SemaphoreSubmitInfo(
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR,
        frame_data[frame_index].swapchain_semaphore);

    VkSemaphoreSubmitInfo signal_semaphore_info =
        vkinit::SemaphoreSubmitInfo(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
                                    frame_data[frame_index].render_semaphore);

    VkSubmitInfo2 submit_info = vkinit::SubmitInfo(
        &cmd_info, &signal_semaphore_info, &wait_semaphore_info);

    VK_CHECK(vkQueueSubmit2(context.graphics_queue, 1, &submit_info,
                            frame_data[frame_index].render_fence));

    VkPresentInfoKHR present_info{};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.pSwapchains = &swapchain.obj;
    present_info.swapchainCount = 1;
    present_info.pWaitSemaphores = &frame_data[frame_index].render_semaphore;
    present_info.waitSemaphoreCount = 1;
    present_info.pImageIndices = &swapchain_image_index;

    {
      VkResult e = vkQueuePresentKHR(context.graphics_queue, &present_info);
      if (e == VK_ERROR_OUT_OF_DATE_KHR) {
        int32_t width, height;
        glfwGetWindowSize(window, &width, &height);
        vkDeviceWaitIdle(context.device);
        DestroyVulkanSwapchain(context, swapchain);
        CreateVulkanSwapchain(context, width, height, swapchain);
      }
    }

    frame_index ^= 1;
  }
}

void Engine::Destroy() {
  vkDeviceWaitIdle(context.device);

  for (FrameData &frame : frame_data) {
    DestroyFrameData(context, frame);
  }

  texture_manager.Destroy(context);
  descriptor_builder.Destroy(context);
  vkDestroyDescriptorSetLayout(context.device, descriptor_layout, nullptr);
  vkDestroyDescriptorSetLayout(context.device, skybox_descriptor_layout,
                               nullptr);
  vkDestroyDescriptorSetLayout(context.device, billboard_descriptor_layout,
                               nullptr);
  vkDestroyDescriptorSetLayout(context.device, shadow_descriptor_set_layout,
                               nullptr);

  camera.Destroy(context);
  DestroyImageSampler(context, sampler);
  DestroyImageSampler(context, shadow_sampler);

  DestroySkybox(context, skybox);

  DestroyBuffer(context, point_light_buffer);
  DestroyBuffer(context, directional_light_buffer);
  DestroyBuffer(context, light_matrix_buffer);
  DestroyBuffer(context, aabb_buffer);

  immediate_submit.Destroy(context);

  DestroyObject(context, cube_obj);
  for (auto &object : scene) {
    DestroyObject(context, object);
  }
  DestroyObject(context, rectangle_obj);

  DestroyAllocatedImage(context, msaa_draw_image);
  DestroyAllocatedImage(context, draw_image);
  DestroyAllocatedImage(context, depth_image);
  DestroyAllocatedImage(context, shadow_image);

  DestroyPipeline(context, mesh_pipeline);
  DestroyPipeline(context, skybox_pipeline);
  DestroyPipeline(context, shadow_pipeline);
  DestroyPipeline(context, aabb_pipeline);

  DestroyVulkanSwapchain(context, swapchain);
  DestroyVulkanContext(context);

  glfwDestroyWindow(window);
  glfwTerminate();
}
