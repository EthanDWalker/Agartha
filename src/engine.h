#pragma once

#include "Backend/allocated_image.h"
#include "Backend/binding_table.h"
#include "Backend/buffer.h"
#include "Backend/context.h"
#include "Backend/descriptors.h"
#include "Backend/immediate_submit.h"
#include "Backend/pipeline.h"
#include "GLFW/glfw3.h"
#include "Managers/light_manager.h"
#include "Managers/scene_manager.h"
#include "Managers/texture_manager.h"
#include "Managers/ui_manager.h"
#include "camera.h"
#include "render_graph.h"

#if defined(DEBUG)
constexpr bool DEBUG = true;
#else
constexpr bool DEBUG = false;
#endif

struct Engine {
  VulkanContext context;

  LightManager light_manager;
  SceneManager scene_manager;
  TextureManager texture_manager;
  UiManager ui_manager;

  DescriptorBuilder descriptor_builder;

  ImmediateSubmit immediate_submit;
  Camera camera;
  RenderGraph render_graph;

  ShaderBindingTable shader_binding_table;

  AllocatedImage main_image;
  AllocatedImage mr_normal_image;
  AllocatedImage depth_image;

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
  Pipeline tone_map_pipeline;

  VkDescriptorSet main_descriptor_set;
  VkDescriptorSetLayout main_descriptor_layout;

  VkDescriptorSet tone_map_descriptor_set;
  VkDescriptorSetLayout tone_map_descriptor_layout;

  VkDescriptorSet ray_tracing_descriptor_set;
  VkDescriptorSetLayout ray_tracing_descriptor_layout;

  VkDescriptorSet cull_descriptor_set;
  VkDescriptorSetLayout cull_descriptor_layout;

  VkDescriptorSet shadow_descriptor_set;
  VkDescriptorSetLayout shadow_descriptor_layout;

  VkDescriptorSet shadow_cull_descriptor_set;
  VkDescriptorSetLayout shadow_cull_descriptor_layout;

  GLFWwindow *window;

  bool tone_mapping_on{true};

  void Init();

  void CreateRenderGraph();

  void Run();

  void Destroy();
};
