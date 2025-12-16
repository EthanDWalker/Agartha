#include "editor.h"
#include "Backend/allocated_image.h"
#include "Backend/buffer.h"
#include "Backend/context.h"
#include "Backend/immediate_submit.h"
#include "Backend/init.h"
#include "Backend/pipeline.h"
#include "Managers/scene_manager.h"
#include "Physics/context.h"
#include "UI/console.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"
#include "input.h"
#include "primitives.h"
#include <cstdint>
#include <cstdlib>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/vector_float4.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/string_cast.hpp>

namespace ui {
void Editor::Resize(AllocatedImage &main_image) {
  ImGui_ImplVulkan_RemoveTexture(main_image_ui_texture);
  main_image_ui_texture = ImGui_ImplVulkan_AddTexture(sampler, main_image.image_view,
                                                      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  resize_requested = false;
}

void Editor::Init(DescriptorBuilder &descriptor_builder, SceneManager &scene_manager,
                  Camera &camera, GLFWwindow *window, AllocatedImage &main_image,
                  VkFormat depth_format, VkFormat swapchain_format) {

  context.Create(window, &swapchain_format, depth_format);
  transformation_widget.Create(descriptor_builder, camera, main_image.format);

  CreateBuffer(sizeof(uint32_t) * SCENE_MAX_INSTANCES,
               VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
               VMA_MEMORY_USAGE_GPU_ONLY, selected_instances_buffer);

  CreateImageSampler(sampler);

  file_explorer.Init(sampler);

  main_image_ui_texture = ImGui_ImplVulkan_AddTexture(sampler, main_image.image_view,
                                                      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

  {
    descriptor_builder.BindStorageBuffer(0, selected_instances_buffer.buffer);
    descriptor_builder.Build(VK_SHADER_STAGE_COMPUTE_BIT, selected_instance_descriptor_set,
                             selected_instance_descriptor_layout);
  }

  {
    GraphicsPipelineBuilder pipeline_builder{};
    pipeline_builder.SetShaders("Debug/outline.vert.spv", "Debug/outline.frag.spv");
    pipeline_builder.Default();
    pipeline_builder.AddColorAttachment(main_image.format);
    pipeline_builder.SetNoDepthTest();
    pipeline_builder.SetPolygonMode(VK_POLYGON_MODE_LINE);
    pipeline_builder.AddDescriptorSetLayout(camera.descriptor_layout);
    pipeline_builder.AddDescriptorSetLayout(scene_manager.object_descriptor_layout);
    pipeline_builder.AddDescriptorSetLayout(scene_manager.instance_descriptor_layout);
    pipeline_builder.Build(outline_pipeline);
  }

  {
    std::array<VkDescriptorSetLayout, 3> ds = {
        scene_manager.instance_descriptor_layout,
        scene_manager.object_descriptor_layout,
        selected_instance_descriptor_layout,
    };

    outline_draw_command.Init(descriptor_builder, "Debug/outline.comp.spv", SCENE_MAX_INSTANCES,
                              ds.data(), ds.size());
  }
}

void Editor::UpdateSelectedInstances(SceneManager &scene_manager, uint32_t selected_instance) {
  if (selected_instance < scene_manager.instance_index) {
    if (!InputContext::GetInputHeld(Input::LEFT_SHIFT)) {
      selected_instances.clear();
    }
    selected_instances.push_back(selected_instance);

    // average matrix
    transformation_widget.matrix =
        (transformation_widget.matrix * static_cast<float>(selected_instances.size() - 1));
    transformation_widget.matrix =
        (transformation_widget.matrix + scene_manager.instances[selected_instance].matrix) /
        static_cast<float>(selected_instances.size());

    {
      uint32_t selected_instances_size = selected_instances.size();

      size_t size = sizeof(uint32_t) * (selected_instances.size() + 1);

      AllocatedBuffer upload_buffer;
      CreateBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU,
                   upload_buffer);

      memcpy(upload_buffer.info.pMappedData, &selected_instances_size, sizeof(uint32_t));
      memcpy(((uint32_t *)upload_buffer.info.pMappedData) + 1, selected_instances.data(),
             selected_instances.size() * sizeof(uint32_t));

      ImmediateSubmit::Submit([&](VkCommandBuffer cmd) {
        VkBufferCopy buffer_copy{};
        buffer_copy.size = size;
        buffer_copy.dstOffset = 0;
        buffer_copy.srcOffset = 0;

        vkCmdCopyBuffer(cmd, upload_buffer.buffer, selected_instances_buffer.buffer, 1,
                        &buffer_copy);
      });

      DestroyBuffer(upload_buffer);
    }

    can_move_transformation_widget = false;
  } else {
    transformation_widget.Hide();
    selected_instances.clear();
    ImmediateSubmit::Submit([this](VkCommandBuffer cmd) {
      vkCmdFillBuffer(cmd, selected_instances_buffer.buffer, 0, selected_instances_buffer.info.size,
                      0);
    });
  }
}

void Editor::BuildDraw(VkCommandBuffer cmd, SceneManager &scene_manager,
                       PhysicsContext &physics_context) {
  {
    std::array<VkDescriptorSet, 3> ds = {
        scene_manager.instance_descriptor_set,
        scene_manager.object_descriptor_set,
        selected_instance_descriptor_set,
    };

    outline_draw_command.BuildDraw(
        cmd, ds.data(), ds.size(),
        {static_cast<uint32_t>(glm::ceil(selected_instances.size() / 16.0f)), 1, 1});
  }
}

void Editor::Draw(VkCommandBuffer cmd, SceneManager &scene_manager, Camera &camera,
                  AllocatedImage &main_image, AllocatedImage &depth_image) {
  VkViewport viewport = vkinit::Viewport(main_image.extent);
  vkCmdSetViewport(cmd, 0, 1, &viewport);
  VkRect2D scissor = vkinit::Scissor(main_image.extent);
  vkCmdSetScissor(cmd, 0, 1, &scissor);

  VkRenderingAttachmentInfo color_att = vkinit::AttachmentInfo(
      main_image.image_view, nullptr, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

  std::array<VkRenderingAttachmentInfo, 1> attachments = {
      color_att,
  };

  VkRenderingAttachmentInfo depth_att =
      vkinit::DepthAttachmentInfo(depth_image.image_view, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                                  VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_NONE);

  VkRenderingInfo rendering_info =
      vkinit::RenderingInfo(main_image.extent, attachments, &depth_att);

  if (!selected_instances.empty()) {
    vkCmdBeginRendering(cmd, &rendering_info);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, outline_pipeline.obj);

    std::array<VkDescriptorSet, 3> ds = {
        camera.descriptor_set,
        scene_manager.object_descriptor_set,
        scene_manager.instance_descriptor_set,
    };

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, outline_pipeline.layout, 0,
                            ds.size(), ds.data(), 0, nullptr);

    vkCmdBindIndexBuffer(cmd, scene_manager.index_buffer.buffer, 0, VK_INDEX_TYPE_UINT32);

    outline_draw_command.Draw(cmd);

    transformation_widget.Draw(cmd, camera);

    vkCmdEndRendering(cmd);
  }
}

void Editor::DrawUI(VkCommandBuffer cmd, VkImage image, VkImageView image_view, VkExtent3D extent) {
  TransitionImage(cmd, {}, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, {},
                  VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, {},
                  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, image);
  VkViewport viewport = vkinit::Viewport(extent);
  vkCmdSetViewport(cmd, 0, 1, &viewport);
  VkRect2D scissor = vkinit::Scissor(extent);
  vkCmdSetScissor(cmd, 0, 1, &scissor);

  VkClearColorValue clear_color_value{};
  clear_color_value = {0.0f, 0.0f, 0.0f, 0.0f};

  VkClearValue clear_value{};
  clear_value.color = clear_color_value;

  VkRenderingAttachmentInfo color_att = vkinit::AttachmentInfo(
      image_view, nullptr, &clear_value, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

  std::array<VkRenderingAttachmentInfo, 1> attachments = {
      color_att,
  };

  VkRenderingInfo rendering_info = vkinit::RenderingInfo(extent, attachments, nullptr);

  vkCmdBeginRendering(cmd, &rendering_info);

  ImGui::Render();
  ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

  vkCmdEndRendering(cmd);
}

void Editor::TrySelectInstance(PhysicsContext &physics_context, SceneManager &scene_manager,
                               Camera &camera) {
  ImVec2 mouse_pos_imvec = ImGui::GetMousePos();
  glm::vec2 mouse_pos = glm::vec2(mouse_pos_imvec.x, mouse_pos_imvec.y);
  glm::vec2 window_middle = scene_window_pos + scene_window_size / 2.0f;
  glm::vec2 mouse_pos_ndc = glm::vec2(mouse_pos - window_middle) / (scene_window_size / 2.0f);

  glm::vec4 view = camera.buffer_data.inv_proj * glm::vec4(mouse_pos_ndc, 1, 1);

  glm::vec4 direction = camera.buffer_data.inv_view * glm::vec4(glm::normalize(glm::vec3(view)), 0);
  RayCastQuery ray_query{};
  ray_query.direction = glm::vec3(direction);
  ray_query.position = camera.position;
  ray_query.tmin = 0.1f;
  ray_query.tmax = 1000.0f;
  uint32_t ray_cast_index = PhysicsQueueRayCast(physics_context, &ray_query);
  physics_context.ray_cast_event.AddListener([ray_cast_index, &physics_context, this,
                                              &scene_manager]() {
    uint32_t selected_instance = physics_context.ray_cast_results[ray_cast_index].instance_index;
    UpdateSelectedInstances(scene_manager, selected_instance);
    return false;
  });
}

void Editor::Transform() {
  glm::mat4 matrix = transformation_widget.matrix;
  glm::vec3 translation = glm::vec3(matrix[3]);
  glm::vec3 scale =
      glm::vec3(glm::length(matrix[0]), glm::length(matrix[1]), glm::length(matrix[2]));
  glm::mat3 rotation_matrix =
      glm::mat3(matrix[0] / scale.x, matrix[1] / scale.y, matrix[2] / scale.z);
  glm::quat rotation = glm::toQuat(rotation_matrix);

  ImGui::BeginGroup();
  {
    ImGui::Text("Transform");
    ImGui::InputFloat3("Translation", (float *)&translation);
    ImGui::InputFloat3("Scale", (float *)&scale);
    ImGui::InputFloat4("Rotation", (float *)&rotation);
  }
  ImGui::EndGroup();

  glm::mat3 new_rotation_matrix = glm::toMat3(rotation);
  new_rotation_matrix[0] *= scale.x;
  new_rotation_matrix[1] *= scale.y;
  new_rotation_matrix[2] *= scale.z;
  glm::mat4 new_matrix =
      glm::mat4(glm::vec4(glm::vec3(new_rotation_matrix[0]), 0.0),
                glm::vec4(glm::vec3(new_rotation_matrix[1]), 0.0),
                glm::vec4(glm::vec3(new_rotation_matrix[2]), 0.0), glm::vec4(translation, 1.0));

  transformation_widget.matrix = new_matrix;
}

void Editor::DrawSceneNode(SceneManager &scene_manager, SceneNode &scene_node) {
  if (scene_node.children.empty()) {
    if (ImGui::Button(scene_node.name.c_str())) {
      uint32_t selected_instance = scene_node.instance_index;
      UpdateSelectedInstances(scene_manager, selected_instance);
    };
  } else if (ImGui::CollapsingHeader(scene_node.name.c_str())) {
    for (auto &child : scene_node.children) {
      DrawSceneNode(scene_manager, child);
    }
  }
}

void Editor::SceneList(SceneManager &scene_manager) {
  if (ImGui::CollapsingHeader("Scene List")) {
    for (auto &scene_node : scene_manager.root_scene_nodes) {
      DrawSceneNode(scene_manager, scene_node);
    }
  }
}

void Editor::Update(PhysicsContext &physics_context, SceneManager &scene_manager,
                    TextureManager &texture_manager, Camera &camera) {
  ImGui_ImplVulkan_NewFrame();
  ImGui_ImplGlfw_NewFrame();

  ImGui::NewFrame();
  ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);
  ImGui::PushFont(NULL, 20.0f);
  if (ImGui::Begin("Inspector")) {
    if (transformation_widget.matrix != glm::mat4(0.0f)) {
      Transform();

      scene_manager.changed_instances.reserve(selected_instances.size());

      glm::mat4 average_matrix = glm::mat4(0.0f);
      for (uint32_t instance_index : selected_instances) {
        average_matrix += scene_manager.instances[instance_index].matrix;
      }
      average_matrix /= selected_instances.size();
      glm::mat4 delta_matrix = transformation_widget.matrix - average_matrix;

      for (uint32_t instance_index : selected_instances) {
        scene_manager.instances[instance_index].matrix += delta_matrix;
        scene_manager.changed_instances.push_back(instance_index);
      }
      scene_manager.UpdateInstances();
    }
  }
  ImGui::End();

  if (ImGui::Begin("Scene Manager")) {
    if (ImGui::Button("Add Cube")) {
      SceneNodeData cube_data = Primitives::GetCubeData();
      scene_manager.AddSceneNode(cube_data, texture_manager);
    }
    SceneList(scene_manager);
  }
  ImGui::End();

  bool scene_focused = false;
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
  if (ImGui::Begin("Scene")) {
    ImVec2 window_region = ImGui::GetContentRegionAvail();
    if (scene_window_size != *(glm::vec2 *)&window_region) {
      resize_requested = true;
      scene_window_size = {window_region.x, window_region.y};
    }

    glm::vec2 offset = {
        ImGui::GetWindowSize().x - ImGui::GetContentRegionAvail().x,
        ImGui::GetWindowSize().y - ImGui::GetContentRegionAvail().y,
    };
    ImVec2 window_pos = ImGui::GetWindowPos();
    scene_window_pos = (*(glm::vec2 *)&window_pos) + offset;

    scene_focused = ImGui::IsWindowFocused();

    ImGui::Image((ImTextureID)main_image_ui_texture, window_region);
    ImGui::PopStyleVar();
  }
  ImGui::End();

  file_explorer.Draw(sampler);

  Console::Draw();

  ImGui::PopFont();

  ImGui::EndFrame();

  if (!scene_focused) {
    return;
  }

  if (!can_move_transformation_widget && !InputContext::GetInputHeld(Input::MOUSE_LEFT)) {
    can_move_transformation_widget = true;
  }
  if (can_move_transformation_widget) {
    ImVec2 mouse_pos_imvec = ImGui::GetMousePos();
    glm::vec2 mouse_pos = glm::vec2(mouse_pos_imvec.x, mouse_pos_imvec.y);
    glm::vec2 window_middle = scene_window_pos + scene_window_size / 2.0f;
    glm::vec2 mouse_pos_ndc = glm::vec2(mouse_pos - window_middle) / (scene_window_size / 2.0f);
    transformation_widget.Update(camera, mouse_pos_ndc);
  }
  if (!transformation_widget.Using() && InputContext::GetInputPressed(Input::MOUSE_LEFT)) {
    TrySelectInstance(physics_context, scene_manager, camera);
  }
}

void Editor::Destroy() {
  file_explorer.Destroy();
  DestroyImageSampler(sampler);
  vkDestroyDescriptorSetLayout(VulkanContext::device, selected_instance_descriptor_layout, nullptr);
  DestroyBuffer(selected_instances_buffer);
  outline_draw_command.Destroy();
  DestroyPipeline(outline_pipeline);
  transformation_widget.Destroy();
  context.Destroy();
}
} // namespace ui
