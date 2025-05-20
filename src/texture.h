#pragma once

#include "Backend/image.h"
#include <string>

static const std::string texture_file_path = "../assets/textures/";

struct Texture {
  AllocatedImage albedo;
  AllocatedImage specular; // TEMP
  AllocatedImage immission;

  std::array<AllocatedImage, 3> ToArray();
};

void CreateTexture(VulkanContext &context, ImmediateSubmit immediate_submit,
                   std::string file_name, std::string file_type,
                   Texture &texture);

void DestroyTexture(VulkanContext &context, Texture &texture);
