#pragma once

#include "Backend/allocated_image.h"
#include "Backend/buffer.h"
#include "Backend/context.h"
#include "Backend/descriptors.h"

struct Noise {
  AllocatedImage obj;
  AllocatedBuffer kernel;
  VkSampler sampler;
};

void CreateNoiseImage(VulkanContext &context,
                      DescriptorBuilder &descriptor_builder, Noise &noise);

void DestroyNoiseImage(VulkanContext &context, Noise &noise);
