#include "image.h"
#include "fmt/base.h"
#include "glm/ext/vector_int2.hpp"
#include <cstdint>
#include <filesystem>
#include <fmt/core.h>

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image.h"
#include "stb_image_resize2.h"

void GetImageInfo(std::string file_name, int32_t *width, int32_t *height) {
  std::string full_path = file_name;
  int32_t comp;
  stbi_info(full_path.c_str(), width, height, &comp);
}

void ResizeImageData(ImageData &image_data, glm::ivec2 forced_extent) {
  const int32_t channel_count = 4;
  void *resized_data =
      malloc(forced_extent.x * forced_extent.y * channel_count);

  stbir_resize_uint8_linear((uint8_t *)image_data.data, image_data.width,
                            image_data.height, 0, (uint8_t *)resized_data,
                            forced_extent.x, forced_extent.y, 0,
                            (stbir_pixel_layout)channel_count);

  DestroyImageData(image_data);
  image_data.data = resized_data;
  image_data.width = forced_extent.x;
  image_data.height = forced_extent.y;
}

void ParseImageData(std::string file_name, ImageData &image_data,
                    bool float_data, bool flip) {
  stbi_set_flip_vertically_on_load(flip);

  std::filesystem::path full_path = file_name;
  std::string path = std::filesystem::absolute(full_path).string();

  if (!std::filesystem::exists(full_path)) {
    fmt::println("ERROR: File does not exist: {}", full_path.string());
    return;
  }

  int32_t channel_count;

  if (float_data) {
    image_data.data = (float *)stbi_loadf(
        path.c_str(), &image_data.width, &image_data.height, &channel_count, 4);
  } else {
    image_data.data = (void *)stbi_load(path.c_str(), &image_data.width,
                                        &image_data.height, &channel_count, 4);
  }

  if (!image_data.data) {
    return fmt::println("texture creation for {} failed :( : {}", path,
                        stbi_failure_reason());
  }
}

void DestroyImageData(ImageData &image_data) {
  stbi_image_free(image_data.data);
}
