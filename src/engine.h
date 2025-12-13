#pragma once

#include "Backend/allocated_image.h"
#include "Backend/binding_table.h"
#include "Backend/descriptors.h"
#include "Backend/immediate_submit.h"
#include "Backend/indirect_draw.h"
#include "Backend/pipeline.h"
#include "GLFW/glfw3.h"
#include "Managers/light_manager.h"
#include "Managers/scene_manager.h"
#include "Managers/texture_manager.h"
#include "Physics/context.h"
#include "UI/editor.h"
#include "camera.h"
#include "render_graph.h"
#include "scene_svo.h"

struct Engine {
  ui::Editor editor;
  PhysicsContext physics_context;

  LightManager light_manager;
  SceneManager scene_manager;
  TextureManager texture_manager;

  SceneSvo scene_svo;

  DescriptorBuilder descriptor_builder;

  Camera camera;
  RenderGraph render_graph;

  ShaderBindingTable shader_binding_table;

  AllocatedImage main_image;
  AllocatedImage mr_normal_image;
  AllocatedImage ao_image;
  AllocatedImage depth_image;
  AllocatedImage skybox_image;

  IndirectDrawIndexedCommand main_draw_command;
  IndirectDrawIndexedCommand shadow_draw_command;

  glm::vec3 sun_direction{-1.0f, -4.0f, -1.0f};

  Pipeline main_pipeline;
  Pipeline shadow_pipeline;
  Pipeline cull_pipeline;
  Pipeline shadow_cull_pipeline;
  Pipeline ray_tracing_pipeline;
  Pipeline tone_map_pipeline;
  Pipeline ambient_occlusion_pipeline;
  Pipeline upscale_ao_pipeline;
  Pipeline depth_pipeline;
  Pipeline atmosphere_pipeline;
  Pipeline skybox_pipeline;

  VkDescriptorSet gbuffer_descriptor_set;
  VkDescriptorSetLayout gbuffer_descriptor_layout;

  VkDescriptorSet ray_tracing_descriptor_set;
  VkDescriptorSetLayout ray_tracing_descriptor_layout;

  VkDescriptorSet skybox_descriptor_set;
  VkDescriptorSetLayout skybox_descriptor_layout;

  VkSampler sampler;

  GLFWwindow *window;

  float delta_time;

  void Init();

  void CreateRenderGraph();

  void Resize(glm::vec2 new_size);

  void Run();

  void Destroy();
};
