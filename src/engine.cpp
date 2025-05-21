#include "engine.h"
#include "Backend/buffer.h"
#include "Backend/context.h"
#include "Backend/descriptors.h"
#include "Backend/frame_data.h"
#include "Backend/allocated_image.h"
#include "Backend/init.h"
#include "Backend/pipeline.h"
#include "Backend/swapchain.h"
#include "Backend/util.h"
#include "Loaders/gltf.h"
#include "cube_data.h"
#include "mesh.h"
#include "texture.h"
#include "types.h"
#include <GLFW/glfw3.h>
#include <cstdint>
#include <limits>
#include <span>
#include <vulkan/vulkan_core.h>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/transform.hpp>

static PointLight point_light{
    .color = {1.0, 1.0, 1.0, 1.0},
    .position = {-0.0, 2.0, 2.0},
};

void Engine::Init() {
  glfwInit();
  glfwWindowHint(GLFW_CLIENT_API, GLFW_FALSE);
  window = glfwCreateWindow(1600, 900, "Engine", nullptr, nullptr);
  InitVulkanContext(window, DEBUG, context);
  CreateVulkanSwapchain(context, 1600, 900, swapchain);

  for (FrameData &frame : frame_data) {
    CreateFrameData(context, frame);
  }

  VkExtent3D draw_image_extent_3d = {
      swapchain.extent.width,
      swapchain.extent.height,
      1,
  };

  CreateAllocatedImage(
      context, draw_image_extent_3d, VK_FORMAT_R16G16B16A16_SFLOAT,
      VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT |
          VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
      draw_image);

  CreateAllocatedImage(context, draw_image_extent_3d, VK_FORMAT_D32_SFLOAT,
                       VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                       depth_image);

  immediate_submit.Create(context);
  descriptor_builder.Init(context);

  CreateImageSampler(context, sampler);
  CreateTexture(context, immediate_submit, "Default", "jpg", box_texture);

  CreateSkybox(context, immediate_submit, descriptor_builder, "sunset", skybox);

  CreateBufferData(context, immediate_submit, &point_light, sizeof(PointLight),
                   VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, point_light_buffer);

  {
    GraphicsPipelineBuilder pipeline_builder;
    pipeline_builder.SetShaders(context, "light.vert.spv", "light.frag.spv");
    pipeline_builder.SetCullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
    pipeline_builder.SetPolygonMode(VK_POLYGON_MODE_FILL);
    pipeline_builder.SetInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    pipeline_builder.SetNoBlending();
    pipeline_builder.SetDepthTest(true, VK_COMPARE_OP_GREATER_OR_EQUAL);
    pipeline_builder.SetDepthFormat(depth_image.format);
    pipeline_builder.SetNoMultisampling();
    pipeline_builder.AddPushConstantRange(VK_SHADER_STAGE_VERTEX_BIT |
                                              VK_SHADER_STAGE_FRAGMENT_BIT,
                                          sizeof(PushConstantData));
    pipeline_builder.Build(context, light_pipeline);
  }

  {
    auto box_texure_images = box_texture.ToArray();
    descriptor_builder.Reset();
    descriptor_builder.BindBuffer(0, point_light_buffer.buffer);
    descriptor_builder.BindSampler(1, sampler);
    descriptor_builder.BindImage(2, skybox.prefilter.image_view);
    descriptor_builder.BindImage(3, skybox.irradiance.image_view);
    descriptor_builder.BindImage(4, skybox.brdf.image_view);
    descriptor_builder.BindImages(5, box_texure_images);
    descriptor_builder.Build(
        context, VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT,
        descriptor_set, descriptor_layout);
  }

  {
    GraphicsPipelineBuilder pipeline_builder;
    pipeline_builder.SetShaders(context, "mesh.vert.spv", "mesh.frag.spv");
    pipeline_builder.SetCullMode(VK_CULL_MODE_FRONT_BIT,
                                 VK_FRONT_FACE_CLOCKWISE);
    pipeline_builder.SetPolygonMode(VK_POLYGON_MODE_FILL);
    pipeline_builder.SetInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    pipeline_builder.SetNoBlending();
    pipeline_builder.SetDepthTest(true, VK_COMPARE_OP_GREATER_OR_EQUAL);
    pipeline_builder.SetDepthFormat(depth_image.format);
    pipeline_builder.SetNoMultisampling();
    pipeline_builder.AddPushConstantRange(VK_SHADER_STAGE_VERTEX_BIT |
                                              VK_SHADER_STAGE_FRAGMENT_BIT,
                                          sizeof(PushConstantData));
    pipeline_builder.AddDescriptorSetLayout(descriptor_layout);
    pipeline_builder.Build(context, mesh_pipeline);
  }

  {
    descriptor_builder.Reset();
    descriptor_builder.BindCombinedImage(0, skybox.image.image_view,
                                         sampler);
    descriptor_builder.Build(
        context, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        skybox_descriptor_set, skybox_descriptor_layout);

    GraphicsPipelineBuilder pipeline_builder;
    pipeline_builder.SetShaders(context, "skybox.vert.spv", "skybox.frag.spv");
    pipeline_builder.SetCullMode(VK_CULL_MODE_BACK_BIT,
                                 VK_FRONT_FACE_CLOCKWISE);
    pipeline_builder.SetPolygonMode(VK_POLYGON_MODE_FILL);
    pipeline_builder.SetInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    pipeline_builder.SetNoBlending();
    pipeline_builder.SetDepthTest(true, VK_COMPARE_OP_GREATER_OR_EQUAL);
    pipeline_builder.SetDepthFormat(depth_image.format);
    pipeline_builder.SetNoMultisampling();
    pipeline_builder.AddPushConstantRange(VK_SHADER_STAGE_VERTEX_BIT |
                                              VK_SHADER_STAGE_FRAGMENT_BIT,
                                          sizeof(SkyboxPushConstantData));
    pipeline_builder.AddDescriptorSetLayout(skybox_descriptor_layout);
    pipeline_builder.Build(context, skybox_pipeline);
  }

  MeshData gltf_data = LoadGltf("DamagedHelmet");
  MeshData cube_data = {};
  cube_data.vertices = cube_vertices;
  cube_data.indices = cube_indices;

  CreateMesh(context, immediate_submit, gltf_data, test_mesh);
  CreateMesh(context, immediate_submit, cube_data, cube_mesh);
  camera.position = {2.0f, 2.0f, 2.0f};
}

void Engine::Run() {
  uint8_t frame_index = 0;

  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
      glfwSetWindowShouldClose(window, true);
    }
    camera.ProcessInput(window, .0001f);
    camera.Update();

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

    VkClearColorValue clear_color_value{};
    clear_color_value = {0.0f, 0.0f, 0.0f};

    VkClearValue clear_value{};
    clear_value.color = clear_color_value;

    VkRenderingAttachmentInfo attachment_info =
        vkinit::AttachmentInfo(draw_image.image_view, &clear_value,
                               VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    VkRenderingAttachmentInfo depth_attachment_info =
        vkinit::DepthAttachmentInfo(depth_image.image_view,
                                    VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

    VkRenderingInfo rendering_info = vkinit::RenderingInfo(
        {draw_image.extent.width, draw_image.extent.height}, &attachment_info,
        &depth_attachment_info);

    vkCmdBeginRendering(cmd, &rendering_info);

    glm::mat4 projection = glm::perspective(
        glm::radians(70.f),
        swapchain.extent.width / static_cast<float>(swapchain.extent.height),
        0.001f, 10000.f);

    projection[1][1] *= -1;

    {
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

      SkyboxPushConstantData pc{};
      pc.proj_matrix = projection;
      pc.view_matrix = camera.GetViewMatrix();
      pc.vertex_buffer = cube_mesh.vertex_address;

      vkCmdPushConstants(cmd, skybox_pipeline.layout,
                         VK_SHADER_STAGE_VERTEX_BIT |
                             VK_SHADER_STAGE_FRAGMENT_BIT,
                         0, sizeof(SkyboxPushConstantData), &pc);

      DrawMesh(cmd, skybox_pipeline, cube_mesh);
    }

    {
      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                        mesh_pipeline.obj);

      PushConstantData pc{};
      pc.vertex_buffer = test_mesh.vertex_address;
      pc.world_matrix = projection * camera.GetViewMatrix();
      pc.view_pos = camera.position;

      vkCmdPushConstants(cmd, mesh_pipeline.layout,
                         VK_SHADER_STAGE_VERTEX_BIT |
                             VK_SHADER_STAGE_FRAGMENT_BIT,
                         0, sizeof(pc), &pc);

      vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                              mesh_pipeline.layout, 0, 1, &descriptor_set, 0,
                              nullptr);

      DrawMesh(cmd, mesh_pipeline, test_mesh);
    }

    {
      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                        light_pipeline.obj);

      PushConstantData pc{};
      pc.vertex_buffer = cube_mesh.vertex_address;
      pc.world_matrix = glm::translate(projection * camera.GetViewMatrix(),
                                       point_light.position);

      vkCmdPushConstants(cmd, light_pipeline.layout,
                         VK_SHADER_STAGE_VERTEX_BIT |
                             VK_SHADER_STAGE_FRAGMENT_BIT,
                         0, sizeof(pc), &pc);

      DrawMesh(cmd, light_pipeline, cube_mesh);
    }

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

  descriptor_builder.Destroy(context);
  vkDestroyDescriptorSetLayout(context.device, descriptor_layout, nullptr);
  vkDestroyDescriptorSetLayout(context.device, skybox_descriptor_layout,
                               nullptr);

  DestroyImageSampler(context, sampler);

  DestroySkybox(context, skybox);

  DestroyTexture(context, box_texture);

  DestroyBuffer(context, point_light_buffer);

  immediate_submit.Destroy(context);

  DestroyMesh(context, cube_mesh);
  DestroyMesh(context, test_mesh);

  DestroyAllocatedImage(context, draw_image);
  DestroyAllocatedImage(context, depth_image);

  DestroyPipeline(context, mesh_pipeline);
  DestroyPipeline(context, light_pipeline);
  DestroyPipeline(context, skybox_pipeline);

  DestroyVulkanSwapchain(context, swapchain);
  DestroyVulkanContext(context);

  glfwDestroyWindow(window);
  glfwTerminate();
}
