#pragma once
#include "context.h"
#include <optional>
#include <string>
#include <vector>

const std::string shader_file_path = "../assets/shaders/";

struct Pipeline {
  VkPipeline obj;
  VkPipelineLayout layout;
};

struct RaytracingPipelineBuilder {
  enum ShaderStages : uint8_t {
    RAY_GEN = 0,
    MISS = 1,
    CLOSEST_HIT = 2,
    SHADER_STAGE_COUNT = 3,
  };

  std::vector<VkPushConstantRange> push_constant_ranges{};
  std::vector<VkDescriptorSetLayout> descriptor_set_layouts{};

  VkRayTracingShaderGroupCreateInfoKHR
      shader_groups[ShaderStages::SHADER_STAGE_COUNT];

  VkShaderModule shader_modules[ShaderStages::SHADER_STAGE_COUNT];

  void SetShaders(VulkanContext &context, std::string ray_gen, std::string miss,
                  std::string closest_hit);

  void AddPushConstantRange(uint32_t size);
  void AddDescriptorSetLayout(VkDescriptorSetLayout layout);

  void Build(VulkanContext &context, uint8_t max_recursion, Pipeline &pipeline);
};

struct ComputePipelineBuilder {
  std::vector<VkPushConstantRange> push_constant_ranges{};
  std::vector<VkDescriptorSetLayout> descriptor_set_layouts{};
  VkShaderModule shader;

  void SetShader(VulkanContext &context, std::string comp);

  void AddPushConstantRange(uint32_t size);
  void AddDescriptorSetLayout(VkDescriptorSetLayout layout);

  void Build(VulkanContext &context, Pipeline &pipeline);
};

struct GraphicsPipelineBuilder {
  std::vector<VkPushConstantRange> push_constant_ranges{};
  std::vector<VkDescriptorSetLayout> descriptor_set_layouts{};
  std::vector<VkPipelineColorBlendAttachmentState> color_attachments{};
  std::vector<VkFormat> color_attachment_formats;
  VkPipelineInputAssemblyStateCreateInfo input_assembly{};
  VkPipelineTessellationStateCreateInfo tessellation{};
  VkPipelineViewportStateCreateInfo viewport{};
  VkPipelineRasterizationStateCreateInfo rasterization{};
  VkPipelineMultisampleStateCreateInfo multisample{};
  VkPipelineDepthStencilStateCreateInfo depth_stencil{};
  VkPipelineDynamicStateCreateInfo dynamic_state{};
  VkPipelineRenderingCreateInfo render_info{};
  VkPipelineViewportStateCreateInfo viewport_state{};
  VkShaderModule vert_shader;
  VkShaderModule frag_shader;
  std::optional<VkShaderModule> geom_shader;

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

  void SetShaders(VulkanContext &context, std::string vert, std::string frag,
                  std::string geom = "");

  void Default();

  void SetInputTopology(VkPrimitiveTopology topology);

  void SetPolygonMode(VkPolygonMode mode);

  void SetViewportCount(uint32_t count);

  void SetCullMode(VkCullModeFlags cull_mode, VkFrontFace front_face);

  void SetNoMultisampling();

  void SetMultisampling(VkSampleCountFlagBits sample_count);

  void SetNoBlending(uint8_t index);

  void SetBlendingAdditive(uint8_t index);

  void SetBlendingAlpha(uint8_t index);

  void AddColorAttachment(VkFormat format);

  void SetNoDepthTest();

  void SetDepthTest(bool depth_write_enable, VkCompareOp op);

  void SetDepthFormat(VkFormat format);

  void AddPushConstantRange(VkShaderStageFlags stage_flags, uint32_t size);

  void AddDescriptorSetLayout(VkDescriptorSetLayout layout);

  void Build(VulkanContext &context, Pipeline &pipeline);
};

void DestroyPipeline(VulkanContext &context, Pipeline &pipeline);
