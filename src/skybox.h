#pragma once

#include "Backend/allocated_image.h"
#include "Backend/context.h"
#include <string>
#include <vulkan/vulkan.h>
#include "Backend/allocated_image.h"
#include "Backend/descriptors.h"

struct Skybox {
  AllocatedImage image;
  AllocatedImage irradiance;
  AllocatedImage prefilter;
  AllocatedImage brdf;
};

void CreateSkybox(VulkanContext &context, ImmediateSubmit &immediate_submit,
                  DesciptorBuilder &descriptor_builder, std::string file_name,
                  Skybox &skybox);

void DestroySkybox(VulkanContext &context, Skybox &skybox);
