#pragma once

#include "Backend/allocated_image.h"
#include "Backend/buffer.h"
#include "Backend/descriptors.h"
#include "Backend/indirect_draw.h"
#include "Managers/scene_manager.h"
#include "Physics/context.h"
#include "UI/Widgets/transformation.h"
#include "UI/context.h"
#include "UI/file_explorer.h"

namespace ui {
struct Editor {
  ui::Context context;
  ui::FileExplorer file_explorer;
  TransformationWidget transformation_widget;

  IndirectDrawIndexedCommand outline_draw_command;
  Pipeline outline_pipeline;

  VkSampler sampler;
  VkDescriptorSet main_image_ui_texture;
  glm::vec2 scene_window_size;
  glm::vec2 scene_window_pos;

  std::vector<uint32_t> selected_instances;
  AllocatedBuffer selected_instances_buffer;
  VkDescriptorSet selected_instance_descriptor_set;
  VkDescriptorSetLayout selected_instance_descriptor_layout;

  bool can_move_transformation_widget;
  bool resize_requested;

  void Init(DescriptorBuilder &descriptor_builder, SceneManager &scene_manager, Camera &camera,
            GLFWwindow *window, AllocatedImage &main_image, VkFormat depth_format,
            VkFormat swapchain_format);

  void Update(PhysicsContext &physics_context, SceneManager &scene_manager, Camera &camera);

  void Resize(AllocatedImage &main_image);

  void UpdateSelectedInstances(SceneManager &scene_manager, uint32_t selected_instance);

  void BuildDraw(VkCommandBuffer cmd, SceneManager &scene_manager, PhysicsContext &physics_context);

  void Draw(VkCommandBuffer cmd, SceneManager &scene_manager, Camera &camera,
            AllocatedImage &main_image, AllocatedImage &depth_image);

  void DrawUI(VkCommandBuffer cmd, VkImage image, VkImageView image_view, VkExtent3D extent);

  void Destroy();

  void DrawSceneNode(SceneManager &scene_manager, SceneNode &scene_node);

  void SceneList(SceneManager &scene_manager);

  void Transform();

  void TrySelectInstance(PhysicsContext &physics_context, SceneManager &scene_managaer,
                         Camera &camera);
};
} // namespace ui
