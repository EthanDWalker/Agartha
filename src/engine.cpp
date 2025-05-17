#include "engine.h"
#include "Backend/buffer.h"
#include "Backend/context.h"
#include "Backend/frame_data.h"
#include "Backend/image.h"
#include "Backend/init.h"
#include "Backend/pipeline.h"
#include "Backend/swapchain.h"
#include "Backend/util.h"
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
    .color = {1.0, 1.0, 1.0, 2.0},
    .position = {-1.0, 2.0, 2.0},
};

static Material material[] = {
    Material{
        .ambient = {0.1f, 0.1f, 0.1f},
        .shininess = 32.0f,
        .diffuse = {0.8f, 0.8f, 0.8f},
        .padding = 0,
        .specular = {0.1f, 0.1f, 0.1f},
    },
};

void DrawMesh(VkCommandBuffer cmd, Pipeline &pipeline, Pipeline &light_pipeline,
              AllocatedImage &draw_image, Mesh &mesh, glm::mat4 camera_matrix,
              glm::vec3 view_pos, VkDescriptorSet *descriptor_set) {
  VkClearColorValue clear_color_value{};
  clear_color_value = {0.0f, 0.0f, 0.0f};

  VkClearValue clear_value{};
  clear_value.color = clear_color_value;

  VkRenderingAttachmentInfo attachment_info =
      vkinit::AttachmentInfo(draw_image.image_view, &clear_value,
                             VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

  VkRenderingInfo rendering_info =
      vkinit::RenderingInfo({draw_image.extent.width, draw_image.extent.height},
                            &attachment_info, nullptr);

  vkCmdBeginRendering(cmd, &rendering_info);

  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.obj);
  VkViewport viewport = {};
  viewport.x = 0;
  viewport.y = 0;
  viewport.width = draw_image.extent.width;
  viewport.height = draw_image.extent.height;
  viewport.minDepth = 0.f;
  viewport.maxDepth = 1.f;

  vkCmdSetViewport(cmd, 0, 1, &viewport);

  VkRect2D scissor = {};
  scissor.offset.x = 0;
  scissor.offset.y = 0;
  scissor.extent.width = draw_image.extent.width;
  scissor.extent.height = draw_image.extent.height;

  vkCmdSetScissor(cmd, 0, 1, &scissor);

  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.layout,
                          0, 1, descriptor_set, 0, nullptr);

  DrawMesh(cmd, pipeline, camera_matrix, view_pos, mesh);

  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, light_pipeline.obj);

  glm::mat4 new_cam_matrix =
      glm::translate(camera_matrix, point_light.position);

  DrawMesh(cmd, light_pipeline, new_cam_matrix, view_pos, mesh);

  vkCmdEndRendering(cmd);
}

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

  immediate_submit.Create(context);

  CreateImageSampler(context, sampler);
  CreateTexture(context, immediate_submit, "wall.png", wall_texture);

  CreateBufferData(context, immediate_submit, &point_light, sizeof(PointLight),
                   VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, point_light_buffer);
  CreateBufferData(context, immediate_submit, &material, sizeof(Material),
                   VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, material_buffer);

  {
    VkDescriptorPoolSize image_sampler_pool_size{};
    image_sampler_pool_size.descriptorCount = 1;
    image_sampler_pool_size.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

    VkDescriptorPoolSize uniform_pool_size{};
    uniform_pool_size.descriptorCount = 2;
    uniform_pool_size.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;

    VkDescriptorPoolSize pool_sizes[] = {
        image_sampler_pool_size,
        uniform_pool_size,
    };

    VkDescriptorPoolCreateInfo descriptor_pool_ci{};
    descriptor_pool_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptor_pool_ci.pPoolSizes = pool_sizes;
    descriptor_pool_ci.poolSizeCount = 2;
    descriptor_pool_ci.maxSets = 1;

    vkCreateDescriptorPool(context.device, &descriptor_pool_ci, nullptr,
                           &descriptor_pool);
  }

  {
    VkDescriptorSetLayoutBinding binding_image{};
    binding_image.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    binding_image.binding = 0;
    binding_image.descriptorCount = 1;
    binding_image.stageFlags =
        VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayoutBinding binding_uniform{};
    binding_uniform.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    binding_uniform.binding = 1;
    binding_uniform.descriptorCount = 1;
    binding_uniform.stageFlags =
        VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayoutBinding binding_uniform_2{};
    binding_uniform_2.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    binding_uniform_2.binding = 2;
    binding_uniform_2.descriptorCount = 1;
    binding_uniform_2.stageFlags =
        VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayoutBinding bindings[] = {binding_image, binding_uniform,
                                               binding_uniform_2};

    VkDescriptorSetLayoutCreateInfo descriptor_layout_ci{};
    descriptor_layout_ci.sType =
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptor_layout_ci.pBindings = bindings;
    descriptor_layout_ci.bindingCount = 3;
    vkCreateDescriptorSetLayout(context.device, &descriptor_layout_ci, nullptr,
                                &descriptor_layout);

    VkDescriptorSetAllocateInfo descriptor_set_alloc_info{};
    descriptor_set_alloc_info.sType =
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descriptor_set_alloc_info.descriptorPool = descriptor_pool;
    descriptor_set_alloc_info.pSetLayouts = &descriptor_layout;
    descriptor_set_alloc_info.descriptorSetCount = 1;

    vkAllocateDescriptorSets(context.device, &descriptor_set_alloc_info,
                             &descriptor_set);

    VkDescriptorImageInfo image_info{};
    image_info.imageView = wall_texture.image.image_view;
    image_info.sampler = sampler;
    image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet write_image{};
    write_image.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write_image.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write_image.descriptorCount = 1;
    write_image.dstBinding = 0;
    write_image.dstSet = descriptor_set;
    write_image.pImageInfo = &image_info;

    VkDescriptorBufferInfo buffer_info{};
    buffer_info.buffer = point_light_buffer.buffer;
    buffer_info.offset = 0;
    buffer_info.range = VK_WHOLE_SIZE;

    VkWriteDescriptorSet write_buffer{};
    write_buffer.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write_buffer.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    write_buffer.descriptorCount = 1;
    write_buffer.dstBinding = 1;
    write_buffer.dstSet = descriptor_set;
    write_buffer.pBufferInfo = &buffer_info;

    VkDescriptorBufferInfo buffer_info_2{};
    buffer_info_2.buffer = material_buffer.buffer;
    buffer_info_2.offset = 0;
    buffer_info_2.range = VK_WHOLE_SIZE;

    VkWriteDescriptorSet write_buffer_2{};
    write_buffer_2.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write_buffer_2.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    write_buffer_2.descriptorCount = 1;
    write_buffer_2.dstBinding = 2;
    write_buffer_2.dstSet = descriptor_set;
    write_buffer_2.pBufferInfo = &buffer_info_2;

    VkWriteDescriptorSet writes[] = {write_buffer, write_image, write_buffer_2};

    vkUpdateDescriptorSets(context.device, 3, writes, 0, nullptr);
  }

  {
    GraphicsPipelineBuilder pipeline_builder;
    pipeline_builder.SetShaders(context, "light.vert.spv", "light.frag.spv");
    pipeline_builder.SetCullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
    pipeline_builder.SetPolygonMode(VK_POLYGON_MODE_FILL);
    pipeline_builder.SetInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    pipeline_builder.SetNoBlending();
    pipeline_builder.SetNoDepthTest();
    pipeline_builder.SetNoMultisampling();
    pipeline_builder.AddPushConstantRange(VK_SHADER_STAGE_VERTEX_BIT |
                                              VK_SHADER_STAGE_FRAGMENT_BIT,
                                          sizeof(PushConstantData));
    pipeline_builder.Build(context, light_pipeline);
  }

  {
    GraphicsPipelineBuilder pipeline_builder;
    pipeline_builder.SetShaders(context, "mesh.vert.spv", "mesh.frag.spv");
    pipeline_builder.SetCullMode(VK_CULL_MODE_FRONT_BIT,
                                 VK_FRONT_FACE_CLOCKWISE);
    pipeline_builder.SetPolygonMode(VK_POLYGON_MODE_FILL);
    pipeline_builder.SetInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    pipeline_builder.SetNoBlending();
    pipeline_builder.SetNoDepthTest();
    pipeline_builder.SetNoMultisampling();
    pipeline_builder.AddPushConstantRange(VK_SHADER_STAGE_VERTEX_BIT |
                                              VK_SHADER_STAGE_FRAGMENT_BIT,
                                          sizeof(PushConstantData));
    pipeline_builder.AddDescriptorSetLayout(descriptor_layout);
    pipeline_builder.Build(context, mesh_pipeline);
  }

  CreateMesh(context, immediate_submit, cube_indices, cube_vertices,
             rectangle_mesh);
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

    glm::mat4 projection = glm::perspective(
        glm::radians(70.f),
        swapchain.extent.width / static_cast<float>(swapchain.extent.height),
        0.1f, 10000.f);

    projection[1][1] *= -1;

    DrawMesh(cmd, mesh_pipeline, light_pipeline, draw_image, rectangle_mesh,
             projection * camera.GetViewMatrix(), camera.position,
             &descriptor_set);

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

  vkDestroyDescriptorPool(context.device, descriptor_pool, nullptr);
  vkDestroyDescriptorSetLayout(context.device, descriptor_layout, nullptr);

  DestroyImageSampler(context, sampler);

  DestroyTexture(context, wall_texture);
  DestroyBuffer(context, point_light_buffer);
  DestroyBuffer(context, material_buffer);

  immediate_submit.Destroy(context);

  DestroyMesh(context, rectangle_mesh);

  DestroyAllocatedImage(context, draw_image);

  DestroyPipeline(context, mesh_pipeline);
  DestroyPipeline(context, light_pipeline);

  DestroyVulkanSwapchain(context, swapchain);
  DestroyVulkanContext(context);

  glfwDestroyWindow(window);
  glfwTerminate();
}
