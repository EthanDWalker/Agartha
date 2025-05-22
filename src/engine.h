#pragma once

#include "Backend/allocated_image.h"
#include "Backend/buffer.h"
#include "Backend/context.h"
#include "Backend/descriptors.h"
#include "Backend/frame_data.h"
#include "Backend/immediate_submit.h"
#include "Backend/pipeline.h"
#include "Backend/swapchain.h"
#include "GLFW/glfw3.h"
#include "camera.h"
#include "material.h"
#include "object.h"
#include "skybox.h"
#include <cstdint>

#if defined(DEBUG)
constexpr bool DEBUG = true;
#else
constexpr bool DEBUG = false;
#endif

constexpr uint8_t FRAME_OVERLAP = 2;

struct Engine {
  VulkanContext context;
  Swapchain swapchain;
  ImmediateSubmit immediate_submit;
  Camera camera;
  DescriptorBuilder descriptor_builder;
  FrameData frame_data[FRAME_OVERLAP];
  GLFWwindow *window;

  Skybox skybox;
  Material box_material;
  VkSampler sampler;
  AllocatedImage msaa_draw_image;
  AllocatedImage draw_image;
  AllocatedImage depth_image;
  AllocatedBuffer point_light_buffer;

  Object test_obj;
  Object cube_obj;
  Object rectangle_obj;

  Pipeline mesh_pipeline;
  Pipeline billboard_pipeline;
  Pipeline skybox_pipeline;

  VkDescriptorSet descriptor_set;
  VkDescriptorSetLayout descriptor_layout;

  VkDescriptorSet skybox_descriptor_set;
  VkDescriptorSetLayout skybox_descriptor_layout;

  VkDescriptorSet billboard_descriptor_set;
  VkDescriptorSetLayout billboard_descriptor_layout;

  void Init();

  void Run();

  void Destroy();
};
