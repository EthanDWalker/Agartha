#pragma once

#include <string>
#include "glm/ext/vector_int2.hpp"

struct ImageData {
  void *data;
  int32_t width;
  int32_t height;
};

void GetImageInfo(std::string file_name, int32_t *width, int32_t *height);

void ParseImageData(std::string file_name, ImageData &image_data,
                    bool float_data = false, bool flip = false);

void ResizeImageData(ImageData &image_data, glm::ivec2 forced_extent);

void DestroyImageData(ImageData &image_data);
