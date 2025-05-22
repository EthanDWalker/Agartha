#pragma once

#include "Backend/allocated_image.h"
#include "Backend/immediate_submit.h"
#include <string>

static const std::string material_file_path = "../assets/textures/";

struct Material {
  AllocatedImage albedo;
  AllocatedImage metal_roughness;
  AllocatedImage emmissive;
  AllocatedImage normal;
  AllocatedImage ambient_occlusion;

  std::array<AllocatedImage, 5> ToArray();
};

void CreateMaterial(VulkanContext &context, ImmediateSubmit immediate_submit,
                   std::string file_name, std::string file_type,
                   Material &material);

void DestroyMaterial(VulkanContext &context, Material &material);
