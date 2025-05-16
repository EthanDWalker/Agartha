#pragma once

#include "Backend/context.h"
#include "Backend/descriptors.h"
#include "Backend/frame_data.h"
#include "Backend/image.h"
#include "Backend/immediate_submit.h"
#include "Backend/swapchain.h"
#include "Backend/pipeline.h"
#include "GLFW/glfw3.h"
#include "mesh.h"
#include <cstdint>

#if defined(DEBUG)
constexpr bool DEBUG = true;
#else
constexpr bool DEBUG = false;
#endif

constexpr uint8_t FRAME_OVERLAP = 2;

struct Engine {
  Mesh rectangle_mesh;
  DescriptorAllocatator descriptor_allocator;
  DescriptorLayoutCache descriptor_layout_cache;
  VulkanContext context;
  Swapchain swapchain;
  AllocatedImage draw_image;
  ImmediateSubmit immediate_submit;
  Pipeline mesh_pipeline;
  FrameData frame_data[FRAME_OVERLAP];
  GLFWwindow *window;

  void Init();

  void Run();

  void Destroy();
};
