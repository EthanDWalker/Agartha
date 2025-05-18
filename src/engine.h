#pragma once

#include "Backend/buffer.h"
#include "Backend/context.h"
#include "Backend/descriptors.h"
#include "Backend/frame_data.h"
#include "Backend/image.h"
#include "Backend/immediate_submit.h"
#include "Backend/pipeline.h"
#include "Backend/swapchain.h"
#include "GLFW/glfw3.h"
#include "camera.h"
#include "mesh.h"
#include "texture.h"
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
  DesciptorBuilder descriptor_builder;
  FrameData frame_data[FRAME_OVERLAP];
  GLFWwindow *window;

  Texture wall_texture;
  Texture specular_wall_texture;
  VkSampler sampler;
  AllocatedImage draw_image;
  AllocatedBuffer point_light_buffer;

  Mesh rectangle_mesh;

  Pipeline mesh_pipeline;
  Pipeline light_pipeline;

  VkDescriptorSet descriptor_set;
  VkDescriptorSetLayout descriptor_layout;

  void Init();

  void Run();

  void Destroy();
};
