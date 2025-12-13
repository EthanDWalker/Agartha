#pragma once
#include "Backend/allocated_image.h"
#include "glm/ext/vector_int2.hpp"
#include <cassert>
#include <filesystem>
#include <future>

namespace ui {
struct FileExplorer {
  const std::filesystem::path root_path = "../assets/";
  const std::filesystem::path default_file_icon_path = "../assets/textures/default_file_icon.png";
  const glm::ivec2 file_icon_size = {128, 128};
  static constexpr uint32_t file_icon_cache_size = 50;

  struct File {
    std::filesystem::path path;
    uint32_t file_icon_cache_index;

    File(const std::filesystem::path &path) {
      this->path = path;
      this->file_icon_cache_index = 0;
    }
  };

  struct Folder {
    std::vector<Folder> sub_folders;
    std::vector<File> files;
    std::filesystem::path path;

    Folder(const std::filesystem::path &path) {
      this->path = path;
      for (auto &entry : std::filesystem::directory_iterator(this->path)) {
        if (std::filesystem::is_directory(entry)) {
          sub_folders.push_back({entry});
        } else {
          files.push_back({entry});
        }
      }
    }

    void Draw(Folder &selected_folder);
  };

  std::vector<Folder> folders;

  std::mutex draw_mutex;
  AllocatedImage file_icon_image_cache[file_icon_cache_size];
  VkDescriptorSet file_icon_descriptor_cache[file_icon_cache_size];
  std::future<void> file_icon_cache_image_futures[file_icon_cache_size];
  File *cached_files[file_icon_cache_size];

  uint32_t file_icon_cache_index;

  Folder selected_folder{root_path};

  void Init(VkSampler &sampler);

  void LoadFileIconImage(File &file);

  void Draw(VkSampler &sampler);

  bool DrawFileIcon(File &file);

  void DrawFolderList(Folder &folder);

  void Destroy();
};
} // namespace ui
