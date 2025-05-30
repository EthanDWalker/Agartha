#pragma once

#include "Backend/allocated_image.h"
#include "Backend/buffer.h"
#include "Backend/context.h"
#include "Backend/descriptors.h"
#include "Backend/immediate_submit.h"
#include "Backend/pipeline.h"
#include "GLFW/glfw3.h"
#include "camera.h"
#include "object.h"
#include "render_graph.h"
#include "skybox.h"
#include "texture_manager.h"
#include "thread_pool.h"
#include "types.h"

#if defined(DEBUG)
constexpr bool DEBUG = true;
#else
constexpr bool DEBUG = false;
#endif

static PointLight point_light{
    .color = {1.0, 1.0, 1.0, 1.0},
    .position = {-0.0, 2.0, 2.0},
};

static DirectionalLight directional_light{
    .direction = {glm::normalize(glm::vec3(-1.0f, -4.0f, -1.0f))},
};

struct Engine {
  VulkanContext context;
  ImmediateSubmit immediate_submit;
  Camera camera;
  TextureManager texture_manager;
  DescriptorBuilder descriptor_builder;
  ThreadPool thread_pool;
  RenderGraph render_graph;

  Skybox skybox;
  VkSampler sampler;
  VkSampler shadow_sampler;

  AllocatedImage draw_image;
  AllocatedImage depth_image;
  AllocatedImage shadow_image;

  AllocatedBuffer point_light_buffer;
  AllocatedBuffer directional_light_buffer;
  AllocatedBuffer light_matrix_buffer;
  AllocatedBuffer aabb_buffer;

  AllocatedBuffer culled_draw_count_buffer;
  AllocatedBuffer culled_object_buffer;

  std::vector<Object> scene;
  Object cube_obj;
  Object rectangle_obj;

  Pipeline mesh_pipeline;
  Pipeline skybox_pipeline;
  Pipeline shadow_pipeline;
  Pipeline aabb_pipeline;
  Pipeline cull_pipeline;

  VkDescriptorSet descriptor_set;
  VkDescriptorSetLayout descriptor_layout;

  VkDescriptorSet skybox_descriptor_set;
  VkDescriptorSetLayout skybox_descriptor_layout;

  VkDescriptorSet billboard_descriptor_set;
  VkDescriptorSetLayout billboard_descriptor_layout;

  VkDescriptorSet shadow_descriptor_set;
  VkDescriptorSetLayout shadow_descriptor_set_layout;

  VkDescriptorSet cull_descriptor_set;
  VkDescriptorSetLayout cull_descriptor_set_layout;

  GLFWwindow *window;

  bool debug_aabb;

  void Init();

  void CreateRenderGraph();

  void Run();

  void Destroy();
};
