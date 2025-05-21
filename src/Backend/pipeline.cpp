#include "pipeline.h"
#include "context.h"
#include "util.h"
#include <cstdint>
#include <cstring>
#include <fmt/base.h>
#include <fstream>
#include <vector>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

bool LoadShaderModule(std::string_view file_path, VkDevice device,
                      VkShaderModule *out_shader_module) {
  // open the file. With cursor at the end
  std::ifstream file(file_path.data(), std::ios::ate | std::ios::binary);

  if (!file.is_open()) {
    return false;
  }

  // find what the size of the file is by looking up the location of the cursor
  // because the cursor is at the end, it gives the size directly in bytes
  size_t fileSize = (size_t)file.tellg();

  // spirv expects the buffer to be on uint32, so make sure to reserve a int
  // vector big enough for the entire file
  std::vector<uint32_t> buffer(fileSize / sizeof(uint32_t));

  // put file cursor at beginning
  file.seekg(0);

  // load the entire file into the buffer
  file.read((char *)buffer.data(), fileSize);

  // now that the file is loaded into the buffer, we can close it
  file.close();

  // create a new shader module, using the buffer we loaded
  VkShaderModuleCreateInfo ci = {};
  ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  ci.pNext = nullptr;

  // codeSize has to be in bytes, so multiply the ints in the buffer by size of
  // int to know the real size of the buffer
  ci.codeSize = buffer.size() * sizeof(uint32_t);
  ci.pCode = buffer.data();

  // check that the creation goes well.
  VkShaderModule shader_module;
  if (vkCreateShaderModule(device, &ci, nullptr, &shader_module) !=
      VK_SUCCESS) {
    return false;
  }
  *out_shader_module = shader_module;
  return true;
}

void ComputePipelineBuilder::SetShader(VulkanContext &context,
                                       std::string comp) {
  if (!LoadShaderModule(shader_file_path + comp, context.device, &shader)) {
    fmt::println("[ERROR] failed to load {}", comp);
  }
}

void ComputePipelineBuilder::AddPushConstantRange(uint32_t size) {
  VkPushConstantRange range{};
  uint32_t offset = 0;
  for (auto &range : push_constant_ranges) {
    offset += range.offset;
  }
  range.offset = offset;
  range.size = size;
  range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
  push_constant_ranges.push_back(range);
}

void ComputePipelineBuilder::AddDescriptorSetLayout(
    VkDescriptorSetLayout layout) {
  descriptor_set_layouts.push_back(layout);
}

void ComputePipelineBuilder::Build(VulkanContext &context, Pipeline &pipeline) {
  VkPipelineLayoutCreateInfo pipeline_layout_ci{};
  pipeline_layout_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipeline_layout_ci.pPushConstantRanges = push_constant_ranges.data();
  pipeline_layout_ci.pushConstantRangeCount = push_constant_ranges.size();
  pipeline_layout_ci.pSetLayouts = descriptor_set_layouts.data();
  pipeline_layout_ci.setLayoutCount = descriptor_set_layouts.size();

  VK_CHECK(vkCreatePipelineLayout(context.device, &pipeline_layout_ci, nullptr,
                                  &pipeline.layout));

  VkPipelineShaderStageCreateInfo shader_ci{};
  shader_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  shader_ci.stage = VK_SHADER_STAGE_COMPUTE_BIT;
  shader_ci.module = shader;
  shader_ci.pName = "main";

  VkComputePipelineCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
  info.layout = pipeline.layout;
  info.stage = shader_ci;

  VK_CHECK(vkCreateComputePipelines(context.device, VK_NULL_HANDLE, 1, &info,
                                    nullptr, &pipeline.obj));

  vkDestroyShaderModule(context.device, shader, nullptr);
}

void GraphicsPipelineBuilder::SetShaders(VulkanContext &context,
                                         std::string vert, std::string frag) {
  if (!LoadShaderModule(shader_file_path + vert, context.device,
                        &vert_shader)) {
    fmt::println("[ERROR] failed to load {}", vert);
  }
  if (!LoadShaderModule(shader_file_path + frag, context.device,
                        &frag_shader)) {
    fmt::println("[ERROR] failed to load {}", frag);
  }
}

void GraphicsPipelineBuilder::SetInputTopology(VkPrimitiveTopology topology) {
  input_assembly.topology = topology;
  input_assembly.primitiveRestartEnable = VK_FALSE;
}

void GraphicsPipelineBuilder::SetPolygonMode(VkPolygonMode mode) {
  rasterization.polygonMode = mode;
  rasterization.lineWidth = 1.0f;
}

void GraphicsPipelineBuilder::SetCullMode(VkCullModeFlags cull_mode,
                                          VkFrontFace front_face) {
  rasterization.cullMode = cull_mode;
  rasterization.frontFace = front_face;
}

void GraphicsPipelineBuilder::SetNoMultisampling() {
  multisample.sampleShadingEnable = VK_FALSE;
  // multisampling defaulted to no multisampling (1 sample per pixel)
  multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
  multisample.minSampleShading = 1.0f;
  multisample.pSampleMask = nullptr;
  // no alpha to coverage either
  multisample.alphaToCoverageEnable = VK_FALSE;
  multisample.alphaToOneEnable = VK_FALSE;
}

void GraphicsPipelineBuilder::SetNoBlending() {
  color_attachment.colorWriteMask =
      VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  color_attachment.blendEnable = VK_FALSE;
}

void GraphicsPipelineBuilder::SetColorAttachmentFormat(VkFormat format) {
  color_attachment_format = format;
  render_info.colorAttachmentCount = 1;
  render_info.pColorAttachmentFormats = &color_attachment_format;
}

void GraphicsPipelineBuilder::SetNoDepthTest() {
  depth_stencil.depthTestEnable = VK_FALSE;
  depth_stencil.depthWriteEnable = VK_FALSE;
  depth_stencil.depthCompareOp = VK_COMPARE_OP_NEVER;
  depth_stencil.depthBoundsTestEnable = VK_FALSE;
  depth_stencil.stencilTestEnable = VK_FALSE;
  depth_stencil.front = {};
  depth_stencil.back = {};
  depth_stencil.minDepthBounds = 0.f;
  depth_stencil.maxDepthBounds = 1.f;
}

void GraphicsPipelineBuilder::SetDepthTest(bool depth_write_enable,
                                           VkCompareOp op) {
  depth_stencil.depthTestEnable = VK_TRUE;
  depth_stencil.depthWriteEnable = depth_write_enable;
  depth_stencil.depthCompareOp = op;
  depth_stencil.depthBoundsTestEnable = VK_FALSE;
  depth_stencil.stencilTestEnable = VK_FALSE;
  depth_stencil.front = {};
  depth_stencil.back = {};
  depth_stencil.minDepthBounds = 0.f;
  depth_stencil.maxDepthBounds = 1.f;
}

void GraphicsPipelineBuilder::SetDepthFormat(VkFormat format) {
  render_info.depthAttachmentFormat = format;
}

void GraphicsPipelineBuilder::AddPushConstantRange(
    VkShaderStageFlags stage_flags, uint32_t size) {
  VkPushConstantRange range{};
  uint32_t offset = 0;
  for (auto range : push_constant_ranges) {
    offset += range.size;
  }
  range.offset = offset;
  range.size = size;
  range.stageFlags = stage_flags;
  push_constant_ranges.push_back(range);
}

void GraphicsPipelineBuilder::AddDescriptorSetLayout(
    VkDescriptorSetLayout layout) {
  descriptor_set_layouts.push_back(layout);
}

void GraphicsPipelineBuilder::Build(VulkanContext &context,
                                    Pipeline &pipeline) {
  VkPipelineLayoutCreateInfo pipeline_layout_ci{};
  pipeline_layout_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipeline_layout_ci.pPushConstantRanges = push_constant_ranges.data();
  pipeline_layout_ci.pushConstantRangeCount = push_constant_ranges.size();
  pipeline_layout_ci.pSetLayouts = descriptor_set_layouts.data();
  pipeline_layout_ci.setLayoutCount = descriptor_set_layouts.size();

  VK_CHECK(vkCreatePipelineLayout(context.device, &pipeline_layout_ci, nullptr,
                                  &pipeline.layout));

  // at the moment we wont support multiple viewports or scissors
  VkPipelineViewportStateCreateInfo viewport_state = {};
  viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewport_state.pNext = nullptr;

  viewport_state.viewportCount = 1;
  viewport_state.scissorCount = 1;

  // setup dummy color blending. We arent using transparent objects yet
  // the blending is just "no blend", but we do write to the color attachment
  VkPipelineColorBlendStateCreateInfo color_blending = {};
  color_blending.sType =
      VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  color_blending.pNext = nullptr;

  color_blending.logicOpEnable = VK_FALSE;
  color_blending.logicOp = VK_LOGIC_OP_COPY;
  color_blending.attachmentCount = 1;
  color_blending.pAttachments = &color_attachment;

  // completely clear VertexInputStateCreateInfo, as we have no need for it
  VkPipelineVertexInputStateCreateInfo vertex_input_info{};
  vertex_input_info.sType =
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

  // build the actual pipeline
  // we now use all of the info structs we have been writing into into this one
  // to create the pipeline
  VkGraphicsPipelineCreateInfo pipeline_ci = {
      .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
  // connect the renderInfo to the pNext extension mechanism
  pipeline_ci.pNext = &render_info;

  VkPipelineShaderStageCreateInfo shader_stages[2] = {};
  shader_stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  shader_stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
  shader_stages[0].module = vert_shader;
  shader_stages[0].pName = "main";

  shader_stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  shader_stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  shader_stages[1].module = frag_shader;
  shader_stages[1].pName = "main";

  pipeline_ci.stageCount = 2;
  pipeline_ci.pStages = shader_stages;
  pipeline_ci.pVertexInputState = &vertex_input_info;
  pipeline_ci.pInputAssemblyState = &input_assembly;
  pipeline_ci.pViewportState = &viewport_state;
  pipeline_ci.pRasterizationState = &rasterization;
  pipeline_ci.pMultisampleState = &multisample;
  pipeline_ci.pColorBlendState = &color_blending;
  pipeline_ci.pDepthStencilState = &depth_stencil;
  pipeline_ci.layout = pipeline.layout;

  VkDynamicState state[] = {VK_DYNAMIC_STATE_VIEWPORT,
                            VK_DYNAMIC_STATE_SCISSOR};

  VkPipelineDynamicStateCreateInfo dynamic_info{};
  dynamic_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamic_info.pDynamicStates = &state[0];
  dynamic_info.dynamicStateCount = 2;

  pipeline_ci.pDynamicState = &dynamic_info;
  if (vkCreateGraphicsPipelines(context.device, VK_NULL_HANDLE, 1, &pipeline_ci,
                                nullptr, &pipeline.obj) != VK_SUCCESS) {
    fmt::println("[ERROR] failed to create pipeline");
  }

  vkDestroyShaderModule(context.device, vert_shader, nullptr);
  vkDestroyShaderModule(context.device, frag_shader, nullptr);
}

void DestroyPipeline(VulkanContext &context, Pipeline &pipeline) {
  vkDestroyPipelineLayout(context.device, pipeline.layout, nullptr);
  vkDestroyPipeline(context.device, pipeline.obj, nullptr);
}
