#include "image.h"
#include "fmt/base.h"
#include <cstdint>

#define STB_IMAGE_IMPLEMENTATION
#include <fmt/core.h>
#include <stb_image.h>

void GetImageInfo(std::string file_name, int32_t *width, int32_t *height) {
  std::string full_path = image_file_path + file_name;
  int32_t comp;
  stbi_info(full_path.c_str(), width, height, &comp);
}

void ParseImageData(std::string file_name, ImageData &image_data,
                   bool float_data, bool flip) {
  stbi_set_flip_vertically_on_load(flip);

  std::string full_path = image_file_path + file_name;

  int32_t channel_count;
  if (float_data) {
    image_data.data =
        (float *)stbi_loadf(full_path.c_str(), &image_data.width,
                            &image_data.height, &channel_count, 4);
  } else {
    image_data.data = (void *)stbi_load(full_path.c_str(), &image_data.width,
                                        &image_data.height, &channel_count, 4);
  }

  if (!image_data.data) {
    return fmt::println("texture creation for {} failed :( : {}", file_name,
                        stbi_failure_reason());
  }
}

void DestroyImageData(ImageData &image_data) {
  stbi_image_free(image_data.data);
}
