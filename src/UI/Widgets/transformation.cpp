#include "transformation.h"
#include "Backend/buffer.h"
#include "Backend/context.h"
#include "Backend/descriptors.h"
#include "Backend/immediate_submit.h"
#include "Backend/pipeline.h"
#include "GLFW/glfw3.h"
#include "Parsers/model.h"
#include "camera.h"
#include "fmt/base.h"
#include "input.h"
#include <cassert>
#include <cstdint>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/vec3.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>

void TransformationWidget::Create(VulkanContext &vulkan_context,
                                  ImmediateSubmit &immediate_submit,
                                  DescriptorBuilder &descriptor_builder,
                                  Camera &camera, VkFormat draw_format) {
  std::vector<MeshData> gltf_data =
      ParseModel("../assets/models/TransformationWidget.gltf");
  MeshData mesh_data = gltf_data[0];

  CreateBufferData(vulkan_context, immediate_submit, mesh_data.indices.data(),
                   sizeof(uint32_t) * mesh_data.indices.size(),
                   VK_BUFFER_USAGE_INDEX_BUFFER_BIT, index_buffer);

  CreateBufferData(vulkan_context, immediate_submit, mesh_data.vertices.data(),
                   sizeof(Vertex) * mesh_data.vertices.size(),
                   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, vertex_buffer);

  bounds_min = mesh_data.aabb_bounds.first * 2.0f;
  bounds_max = mesh_data.aabb_bounds.second * 2.0f;

  CreateBufferData(vulkan_context, immediate_submit,
                   (void *)DIRECTION_INSTANCES,
                   sizeof(glm::mat4) * TRANSFORMATION_DIRECTION_COUNT,
                   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, instance_buffer);
  CreateBufferData(vulkan_context, immediate_submit, (void *)DIRECTION_COLORS,
                   sizeof(glm::vec4) * TRANSFORMATION_DIRECTION_COUNT,
                   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, color_buffer);

  matrix = glm::mat4(1.0f);

  descriptor_builder.BindStorageBuffer(0, instance_buffer.buffer);
  descriptor_builder.BindStorageBuffer(1, vertex_buffer.buffer);
  descriptor_builder.BindStorageBuffer(2, color_buffer.buffer);
  descriptor_builder.Build(
      vulkan_context, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
      descriptor_set, descriptor_layout);

  GraphicsPipelineBuilder pipeline_builder{};
  pipeline_builder.SetShaders(vulkan_context, "Debug/widget.vert.spv",
                              "Debug/widget.frag.spv");
  pipeline_builder.Default();
  pipeline_builder.SetNoDepthTest();
  pipeline_builder.AddColorAttachment(draw_format);
  pipeline_builder.AddDescriptorSetLayout(descriptor_layout);
  pipeline_builder.AddDescriptorSetLayout(camera.descriptor_layout);
  pipeline_builder.AddPushConstantRange(VK_SHADER_STAGE_VERTEX_BIT,
                                        sizeof(glm::mat4));
  pipeline_builder.Build(vulkan_context, draw_pipeline);
}

void TransformationWidget::Update(GLFWwindow *window, Camera &camera) {
  if (InputContext::GetInputHeld(Input::ONE)) {
    selected_mode = TransformationMode::MOVE;
  } else if (InputContext::GetInputPressed(Input::TWO)) {
    selected_mode = TransformationMode::ROTATE;
  } else if (InputContext::GetInputPressed(Input::THREE)) {
    selected_mode = TransformationMode::SCALE;
  }

  if (!InputContext::GetInputHeld(Input::MOUSE_LEFT)) {
    selected_direction = TransformationDirections::COUNT;
    return;
  }

  glm::mat4 view_proj =
      camera.buffer_data.projection_matrix * camera.buffer_data.view_matrix;

  if (selected_direction == TransformationDirections::COUNT) {
    for (uint32_t i = 0; i < TRANSFORMATION_DIRECTION_COUNT; i++) {
      glm::vec4 bounds_world_min =
          matrix * (DIRECTION_INSTANCES[i] * glm::vec4(bounds_min, 1.0));

      glm::vec4 bounds_world_max =
          matrix * (DIRECTION_INSTANCES[i] * glm::vec4(bounds_max, 1.0));

      glm::vec4 bounds_ndc_min = view_proj * bounds_world_min;
      bounds_ndc_min /= bounds_ndc_min.w;
      glm::vec4 bounds_ndc_max = view_proj * bounds_world_max;
      bounds_ndc_max /= bounds_ndc_max.w;

      glm::vec2 pmin = glm::min(bounds_ndc_min, bounds_ndc_max);
      glm::vec2 pmax = glm::max(bounds_ndc_min, bounds_ndc_max);

      if (glm::all(glm::lessThan(InputContext::mouse_position, pmax)) &&
          glm::all(glm::greaterThan(InputContext::mouse_position, pmin))) {
        selected_direction = static_cast<TransformationDirections>(i);
        break;
      }
    }
    if (selected_direction == TransformationDirections::COUNT) {
      return;
    }
  }

  switch (selected_mode) {
  case (TransformationMode::MOVE): {
    glm::mat4 inv_view_proj = glm::inverse(view_proj);
    glm::vec2 ndc = InputContext::mouse_position;

    glm::vec4 near_clip = glm::vec4(ndc, 0.0f, 1.0f);
    glm::vec4 far_clip = glm::vec4(ndc, 1.0f, 1.0f);

    glm::vec4 near_world4 = inv_view_proj * near_clip;
    glm::vec4 far_world4 = inv_view_proj * far_clip;

    glm::vec3 near_world = glm::vec3(near_world4) / near_world4.w;
    glm::vec3 far_world = glm::vec3(far_world4) / far_world4.w;

    glm::vec3 ray_origin = near_world;
    glm::vec3 ray_dir = glm::normalize(far_world - near_world);

    glm::vec3 plane_normal = DIRECTION_PLANES[selected_direction];
    glm::vec3 plane_point = matrix * glm::vec4(glm::vec3(0.0f), 1.0f);
    glm::vec3 widget_offset = DIRECTION_INSTANCES[selected_direction] *
                              glm::vec4(glm::vec3(0.0f), 1.0f);

    float denom = glm::dot(ray_dir, plane_normal);

    float t = glm::dot(plane_point - ray_origin, plane_normal) / denom;
    glm::vec3 intersection = ray_origin + t * ray_dir;

    matrix[3][selected_direction] =
        intersection[selected_direction] -
        DIRECTION_INSTANCES[selected_direction][3][selected_direction];
    break;
  }
  case (TransformationMode::ROTATE): {
    glm::vec4 widget_pos = view_proj * matrix *
                           (DIRECTION_INSTANCES[selected_direction] *
                            glm::vec4(glm::vec3(0.0f), 1.0f));
    widget_pos /= widget_pos.w;

    glm::vec2 direction = glm::vec2(widget_pos) - InputContext::mouse_position;

    const float rotation_angle =
        glm::length(direction) * glm::sign(abs(direction.x) > abs(direction.y)
                                               ? direction.x
                                               : direction.y);

    const float sin = glm::sin(rotation_angle);
    const float cos = glm::cos(rotation_angle);
    glm::mat3 rotation_matrix;
    switch (selected_direction) {
    case (TransformationDirections::X): {
      rotation_matrix = glm::mat3(glm::vec3(1, 0, 0), glm::vec3(0, cos, -sin),
                                  glm::vec3(0, sin, cos));
      break;
    }
    case (TransformationDirections::Y): {
      rotation_matrix = glm::mat3(glm::vec3(cos, 0, sin), glm::vec3(0, 1, 0),
                                  glm::vec3(-sin, 0, cos));
      break;
    }
    case (TransformationDirections::Z): {
      rotation_matrix = glm::mat3(glm::vec3(cos, -sin, 0),
                                  glm::vec3(sin, cos, 0), glm::vec3(0, 0, 1));
      break;
    }
    default: {
      return;
    }
    }
    matrix[0] = glm::vec4(rotation_matrix[0], matrix[0][3]);
    matrix[1] = glm::vec4(rotation_matrix[1], matrix[1][3]);
    matrix[2] = glm::vec4(rotation_matrix[2], matrix[2][3]);
    break;
  }
  case (TransformationMode::SCALE): {
    glm::vec4 widget_pos = view_proj * matrix *
                           (DIRECTION_INSTANCES[selected_direction] *
                            glm::vec4(glm::vec3(0.0f), 1.0f));
    widget_pos /= widget_pos.w;

    glm::vec2 direction = glm::vec2(widget_pos) - InputContext::mouse_position;
    const float scale_factor =
        glm::length(direction) * glm::sign(abs(direction.x) > abs(direction.y)
                                               ? direction.x
                                               : direction.y) +
        1.0f;
    matrix[selected_direction][selected_direction] =
        sqrt(matrix[selected_direction][selected_direction]) * scale_factor;
    break;
  }
  default: {
    assert(0 &&
           "Must set translation mode to one the the enum values as defined in "
           "the TransformationWidget struct");
  }
  }
}

void TransformationWidget::Draw(VkCommandBuffer cmd, Camera &camera) {
  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, draw_pipeline.obj);

  std::array<VkDescriptorSet, 2> ds = {
      descriptor_set,
      camera.descriptor_set,
  };

  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          draw_pipeline.layout, 0, ds.size(), ds.data(), 0,
                          nullptr);

  vkCmdBindIndexBuffer(cmd, index_buffer.buffer, 0, VK_INDEX_TYPE_UINT32);

  vkCmdPushConstants(cmd, draw_pipeline.layout, VK_SHADER_STAGE_VERTEX_BIT, 0,
                     sizeof(glm::mat4), &matrix);

  vkCmdDrawIndexed(cmd, index_buffer.info.size / sizeof(uint32_t),
                   TRANSFORMATION_DIRECTION_COUNT, 0, 0, 0);
}

void TransformationWidget::Destroy(VulkanContext &vulkan_context) {
  DestroyPipeline(vulkan_context, draw_pipeline);
  DestroyBuffer(vulkan_context, instance_buffer);
  DestroyBuffer(vulkan_context, vertex_buffer);
  DestroyBuffer(vulkan_context, index_buffer);
  DestroyBuffer(vulkan_context, color_buffer);
  vkDestroyDescriptorSetLayout(vulkan_context.device, descriptor_layout,
                               nullptr);
}
