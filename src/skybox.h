#pragma once

#include "Backend/allocated_image.h"
#include "Backend/context.h"
#include "Backend/descriptors.h"
#include "Backend/immediate_submit.h"
#include "Managers/texture_manager.h"
#include <string>
#include <vulkan/vulkan.h>

struct Skybox {
  AllocatedImage image;
  AllocatedImage irradiance;
  AllocatedImage prefilter;
  AllocatedImage brdf;
  VkDescriptorSet descriptor_set;
  VkDescriptorSetLayout descriptor_layout;
  VkSampler sampler;
};

void CreateSkybox(VulkanContext &context, ImmediateSubmit &immediate_submit,
                  DescriptorBuilder &descriptor_builder,
                  TextureManager &texture_manager, std::string file_name,
                  Skybox &skybox);

void DestroySkybox(VulkanContext &context, Skybox &skybox);
