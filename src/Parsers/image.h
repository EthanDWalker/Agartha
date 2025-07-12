#pragma once

#include <string>

struct ImageData {
  void *data;
  int32_t width;
  int32_t height;
};

void GetImageInfo(std::string file_name, int32_t *width, int32_t *height);

void ParseImageData(std::string file_name, ImageData &image_data,
                   bool float_data = false, bool flip = false);

void DestroyImageData(ImageData &image_data);
