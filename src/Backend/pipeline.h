#pragma once
#include "context.h"
#include <string>
#include <vector>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

const std::string shader_file_path = "../assets/shaders/";

struct Pipeline {
  VkPipeline obj;
  VkPipelineLayout layout;
};

struct GraphicsPipelineBuilder {
  std::vector<VkPushConstantRange> push_constant_ranges{};
  std::vector<VkDescriptorSetLayout> descriptor_set_layouts{};
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

  void SetShaders(VulkanContext &context, std::string vert, std::string frag);

  void SetInputTopology(VkPrimitiveTopology topology);

  void SetPolygonMode(VkPolygonMode mode);

  void SetCullMode(VkCullModeFlags cull_mode, VkFrontFace front_face);

  void SetNoMultisampling();

  void SetNoBlending();

  void SetColorAttachmentFormat(VkFormat format);

  void SetNoDepthTest();

  void AddPushConstantRange(VkShaderStageFlags stage_flags, uint32_t size);

  void AddDescriptorSetLayout(VkDescriptorSetLayout layout);

  void Build(VulkanContext &context, Pipeline &pipeline);
};

void DestroyPipeline(VulkanContext &context, Pipeline &pipeline);
