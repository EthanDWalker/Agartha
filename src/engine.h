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
  GLFWwindow *window;

  RenderGraph render_graph;

  Skybox skybox;
  Material box_material;
  VkSampler sampler;
  VkSampler shadow_sampler;

  AllocatedImage shadow_image;

  AllocatedBuffer point_light_buffer;
  AllocatedBuffer directional_light_buffer;
  AllocatedBuffer light_matrix_buffer;
  AllocatedBuffer aabb_buffer;

  std::vector<Object> scene;
  Object cube_obj;
  Object rectangle_obj;

  Pipeline mesh_pipeline;
  Pipeline skybox_pipeline;
  Pipeline shadow_pipeline;
  Pipeline aabb_pipeline;

  VkDescriptorSet descriptor_set;
  VkDescriptorSetLayout descriptor_layout;

  VkDescriptorSet skybox_descriptor_set;
  VkDescriptorSetLayout skybox_descriptor_layout;

  VkDescriptorSet billboard_descriptor_set;
  VkDescriptorSetLayout billboard_descriptor_layout;

  VkDescriptorSet shadow_descriptor_set;
  VkDescriptorSetLayout shadow_descriptor_set_layout;

  void Init();

  void CreateRenderGraph();

  void Run();

  void Destroy();
};
