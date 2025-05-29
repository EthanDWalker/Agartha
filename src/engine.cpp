#include "engine.h"
#include "Backend/allocated_image.h"
#include "Backend/buffer.h"
#include "Backend/context.h"
#include "Backend/descriptors.h"
#include "Backend/pipeline.h"
#include "Loaders/model.h"
#include "Primitives/cube.h"
#include "object.h"
#include "render_graph.h"
#include "texture_manager.h"
#include "types.h"
#include <GLFW/glfw3.h>
#include <cstdint>
#include <vector>
#include <vulkan/vulkan_core.h>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>
#include <glm/gtx/transform.hpp>

void Engine::CreateRenderGraph() { render_graph.Init(context, window); }

void Engine::Init() {
  glfwInit();
  glfwWindowHint(GLFW_CLIENT_API, GLFW_FALSE);
  window = glfwCreateWindow(1600, 900, "Engine", nullptr, nullptr);

  InitVulkanContext(window, DEBUG, context);

  immediate_submit.Create(context);
  descriptor_builder.Init(context);
  texture_manager.Init(context, descriptor_builder);
  camera.Create(context, descriptor_builder);

  CreateAllocatedImage(context, {1024 * 2, 1024 * 2, 1}, VK_FORMAT_D32_SFLOAT,
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

  CreateRenderGraph();
}

void Engine::Run() {
  const float distance = 200.0f;
  const float scene_extent = 85.0f;
  const float far_plane = 240.0f;
  const float near_plane = 0.1f;

  {
    glm::vec3 light_dir = normalize(glm::vec3(-1.0f, -4.0f, -1.0f));
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

    render_graph.Render(context);
    if (render_graph.resize_requested == true) {
      render_graph.Resize(context, window);
    }
  }
}

void Engine::Destroy() {
  vkDeviceWaitIdle(context.device);

  render_graph.Destroy(context);

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

  DestroyAllocatedImage(context, shadow_image);

  DestroyPipeline(context, mesh_pipeline);
  DestroyPipeline(context, skybox_pipeline);
  DestroyPipeline(context, shadow_pipeline);
  DestroyPipeline(context, aabb_pipeline);

  DestroyVulkanContext(context);

  glfwDestroyWindow(window);
  glfwTerminate();
}
