#pragma once

#include <string>

static const std::string image_file_path = "../assets/textures/";

struct ImageData {
  void *data;
  int32_t width;
  int32_t height;
};

void GetImageInfo(std::string file_name, int32_t *width, int32_t *height);

void LoadImageData(std::string file_name, ImageData &image_data,
                   bool float_data = false, bool flip = false);

void DestroyImageData(ImageData &image_data);
