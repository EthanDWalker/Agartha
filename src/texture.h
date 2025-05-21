#pragma once

#include "Backend/allocated_image.h"
#include "Backend/immediate_submit.h"
#include <string>

static const std::string texture_file_path = "../assets/textures/";

struct Texture {
  AllocatedImage albedo;
  AllocatedImage metal_roughness;
  AllocatedImage emmissive;
  AllocatedImage normal;
  AllocatedImage ambient_occlusion;

  std::array<AllocatedImage, 5> ToArray();
};

void CreateTexture(VulkanContext &context, ImmediateSubmit immediate_submit,
                   std::string file_name, std::string file_type,
                   Texture &texture);

void DestroyTexture(VulkanContext &context, Texture &texture);
