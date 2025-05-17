#pragma once

#include "Backend/image.h"
#include <string>

static const std::string texture_file_path = "../assets/textures/";

struct Texture {
  AllocatedImage image;
};

void CreateTexture(VulkanContext &context, ImmediateSubmit immediate_submit,
                   std::string file_path, Texture &texture);

void DestroyTexture(VulkanContext &context, Texture &texture);
