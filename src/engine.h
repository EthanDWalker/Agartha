#pragma once

#include "Backend/allocated_image.h"
#include "Backend/binding_table.h"
#include "Backend/buffer.h"
#include "Backend/context.h"
#include "Backend/descriptors.h"
#include "Backend/immediate_submit.h"
#include "Backend/pipeline.h"
#include "GLFW/glfw3.h"
#include "Managers/scene_manager.h"
#include "Managers/texture_manager.h"
#include "Managers/ui_manager.h"
#include "camera.h"
#include "render_graph.h"
#include "types.h"

#if defined(DEBUG)
constexpr bool DEBUG = true;
#else
constexpr bool DEBUG = false;
#endif

static PointLight point_light{
    .color = {1.0, 1.0, 1.0, 1.0},
    .position = {0.0, 2.0, 2.0},
};

static DirectionalLight directional_light{
    .direction = {
        glm::vec4(glm::normalize(glm::vec3(-1.0f, -4.0f, -1.0f)), 1.0)}};

struct Engine {
  VulkanContext context;
  ImmediateSubmit immediate_submit;
  Camera camera;
  TextureManager texture_manager;
  SceneManager scene_manager;
  UiManager ui_manager;
  DescriptorBuilder descriptor_builder;
  RenderGraph render_graph;

  ShaderBindingTable shader_binding_table;

  AllocatedImage main_image;
  AllocatedImage mr_normal_image;
  AllocatedImage depth_image;
  AllocatedImage shadow_image;

  AllocatedBuffer point_light_buffer;
  AllocatedBuffer directional_light_buffer;
  AllocatedBuffer light_matrix_buffer;

  AllocatedBuffer culled_draw_count_buffer;
  AllocatedBuffer draw_indirect_buffer;
  AllocatedBuffer visible_instance_buffer;

  AllocatedBuffer shadow_culled_draw_count_buffer;
  AllocatedBuffer shadow_draw_indirect_buffer;
  AllocatedBuffer shadow_visible_instance_buffer;

  Pipeline main_pipeline;
  Pipeline shadow_pipeline;
  Pipeline cull_pipeline;
  Pipeline shadow_cull_pipeline;
  Pipeline ray_tracing_pipeline;

  VkSampler shadow_sampler;

  VkDescriptorSet main_descriptor_set;
  VkDescriptorSetLayout main_descriptor_layout;

  VkDescriptorSet ray_tracing_descriptor_set;
  VkDescriptorSetLayout ray_tracing_descriptor_layout;

  VkDescriptorSet cull_descriptor_set;
  VkDescriptorSetLayout cull_descriptor_layout;

  VkDescriptorSet shadow_descriptor_set;
  VkDescriptorSetLayout shadow_descriptor_layout;

  VkDescriptorSet shadow_cull_descriptor_set;
  VkDescriptorSetLayout shadow_cull_descriptor_layout;

  VkDescriptorSet light_descriptor_set;
  VkDescriptorSetLayout light_descriptor_layout;

  GLFWwindow *window;

  void Init();

  void CreateRenderGraph();

  void Run();

  void Destroy();
};
