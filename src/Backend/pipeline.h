#pragma once
#include "context.h"
#include <string>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

constexpr std::string shader_file_path = "../shaders/";

struct Pipeline {
  VkPipeline obj;
  VkPipelineLayout layout;
};

struct GraphicsPipelineBuilder {
  VkPipelineInputAssemblyStateCreateInfo input_assembly{};
  VkPipelineTessellationStateCreateInfo tessellation{};
  VkPipelineViewportStateCreateInfo viewport{};
  VkPipelineRasterizationStateCreateInfo rasterization{};
  VkPipelineMultisampleStateCreateInfo multisample{};
  VkPipelineDepthStencilStateCreateInfo depth_stencil{};
  VkPipelineColorBlendAttachmentState color_attachment{};
  VkPipelineDynamicStateCreateInfo dynamic_state{};
  VkPipelineRenderingCreateInfo render_info{};
  VkFormat color_attachment_format;
  VkShaderModule vert_shader;
  VkShaderModule frag_shader;

  GraphicsPipelineBuilder() {
    render_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    input_assembly.sType =
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    tessellation.sType =
        VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;
    viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    rasterization.sType =
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    multisample.sType =
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    depth_stencil.sType =
        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  }
};

namespace pipeline {
void SetShaders(VulkanContext &context, std::string vert, std::string frag,
                GraphicsPipelineBuilder &builder);

void SetInputTopology(VkPrimitiveTopology topology,
                      GraphicsPipelineBuilder &builder);

void SetPolygonMode(VkPolygonMode mode, GraphicsPipelineBuilder &builder);

void SetCullMode(VkCullModeFlags cull_mode, VkFrontFace front_face,
                 GraphicsPipelineBuilder &builder);

void SetNoMultisampling(GraphicsPipelineBuilder &builder);

void SetNoBlending(GraphicsPipelineBuilder &builder);

void SetColorAttachmentFormat(VkFormat format,
                              GraphicsPipelineBuilder &builder);

void SetNoDepthTest(GraphicsPipelineBuilder &builder);

void BuildGraphicsPipeline(VulkanContext &context,
                           GraphicsPipelineBuilder &builder,
                           Pipeline &pipeline);

void DestroyPipeline(VulkanContext &context, Pipeline &pipeline);
} // namespace pipeline
