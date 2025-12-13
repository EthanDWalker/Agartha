#include "file_explorer.h"
#include "Backend/init.h"
#include "Backend/immediate_submit.h"
#include "Managers/texture_manager.h"
#include "Parsers/image.h"
#include "fmt/base.h"
#include "fmt/format.h"
#include "glm/common.hpp"
#include "imgui.h"
#include "imgui_impl_vulkan.h"
#include <chrono>
#include <filesystem>
#include <future>
#include <mutex>

namespace ui {
void FileExplorer::Init(VkSampler &sampler) {
  for (auto &entry : std::filesystem::directory_iterator(root_path)) {
    if (std::filesystem::is_directory(entry)) {
      folders.push_back({entry});
    }
  }
  memset(file_icon_descriptor_cache, 0,
         sizeof(file_icon_descriptor_cache[0]) * file_icon_cache_size);
  memset(file_icon_image_cache, 0, sizeof(file_icon_image_cache[0]) * file_icon_cache_size);
  memset(cached_files, 0, sizeof(cached_files[0]) * file_icon_cache_size);

  TextureManager::LoadTexture(default_file_icon_path.string(),
                              file_icon_image_cache[file_icon_cache_index], file_icon_size);
  file_icon_descriptor_cache[file_icon_cache_index] =
      ImGui_ImplVulkan_AddTexture(sampler, file_icon_image_cache[file_icon_cache_index].image_view,
                                  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  file_icon_cache_index++;
  VkExtent3D file_icon_extent = {
      static_cast<uint32_t>(file_icon_size.x),
      static_cast<uint32_t>(file_icon_size.y),
      1,
  };
  VkImageMemoryBarrier2 memory_barriers[file_icon_cache_size - 1];
  for (uint32_t i = file_icon_cache_index; i < file_icon_cache_size; i++) {
    CreateAllocatedImage(file_icon_extent, VK_FORMAT_R8G8B8A8_UNORM,
                         VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                         file_icon_image_cache[i]);
    file_icon_descriptor_cache[i] = ImGui_ImplVulkan_AddTexture(
        sampler, file_icon_image_cache[i].image_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    memory_barriers[i - 1] = {};
    memory_barriers[i - 1].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    memory_barriers[i - 1].image = file_icon_image_cache[i].image;
    memory_barriers[i - 1].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    memory_barriers[i - 1].subresourceRange =
        vkinit::ImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT);
    memory_barriers[i - 1].dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
    memory_barriers[i - 1].dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
  }

  ImmediateSubmit::Submit([memory_barriers](VkCommandBuffer cmd) {
    VkDependencyInfo dep_info{};
    dep_info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dep_info.imageMemoryBarrierCount = file_icon_cache_size - 1;
    dep_info.pImageMemoryBarriers = memory_barriers;

    vkCmdPipelineBarrier2(cmd, &dep_info);
  });
}

void FileExplorer::LoadFileIconImage(File &file) {
  if (file.path.extension().string() != ".png" && file.path.extension().string() != ".jpg") {
    return;
  }

  uint32_t cache_index = file_icon_cache_index;

  if (file_icon_cache_image_futures[cache_index].valid()) {
    if (file_icon_cache_image_futures[cache_index].wait_for(std::chrono::seconds(0)) !=
        std::future_status::ready) {
      return;
    }
  }

  file_icon_cache_index = (file_icon_cache_index % (file_icon_cache_size - 1)) + 1;

  if (cached_files[cache_index]) {
    cached_files[cache_index]->file_icon_cache_index = 0;
  }
  cached_files[cache_index] = &file;

  file.file_icon_cache_index = cache_index;

  file_icon_cache_image_futures[cache_index] = std::async(std::launch::async, [=, this]() {
    ImageData image_data;
    ParseImageData(file.path.string(), image_data);
    ResizeImageData(image_data, file_icon_size);
    const uint8_t channel_count = 4;
    {
      std::lock_guard<std::mutex> lock(draw_mutex);
      UpdateImageAsync(file_icon_image_cache[cache_index], image_data.data, channel_count);
    }
    DestroyImageData(image_data);
  });
}

void FileExplorer::Folder::Draw(Folder &selected_folder) {
  bool header_open = ImGui::CollapsingHeader(path.filename().string().c_str());
  if (ImGui::IsItemClicked()) {
    selected_folder = *this;
  }
  if (header_open) {
    for (auto &folder : sub_folders) {
      folder.Draw(selected_folder);
    }
  }
}

bool FileExplorer::DrawFileIcon(File &file) {
  ImGui::PushID(file.path.string().c_str());

  ImGui::BeginGroup();
  ImGui::Selectable("##bg", false, ImGuiSelectableFlags_AllowOverlap,
                    ImVec2(0, file_icon_size.y + 30));

  ImVec2 start = ImGui::GetItemRectMin();

  float image_offset = (ImGui::GetContentRegionAvail().x - file_icon_size.x) * 0.5f;
  ImGui::SetCursorScreenPos({start.x + image_offset, start.y + 4});

  {
    std::lock_guard<std::mutex> lock(draw_mutex);
    ImGui::Image((ImTextureID)file_icon_descriptor_cache[file.file_icon_cache_index],
                 ImVec2(file_icon_size.x, file_icon_size.y));
  }

  ImGui::SetCursorScreenPos({start.x + 4, start.y + file_icon_size.y + 8});
  ImGui::TextWrapped("%s", file.path.filename().string().c_str());

  bool clicked = ImGui::IsItemClicked();
  bool hovered = ImGui::IsItemHovered();

  ImGui::EndGroup();
  ImGui::PopID();

  return clicked;
}

void FileExplorer::Draw(VkSampler &sampler) {
  if (ImGui::Begin("File Explorer")) {
    {
      if (ImGui::BeginChild("Folders", ImVec2(150, 0),
                            ImGuiChildFlags_ResizeX | ImGuiChildFlags_Borders)) {
        for (auto &folder : folders) {
          folder.Draw(selected_folder);
        }
      }
      ImGui::EndChild();
    }
    ImGui::SameLine();
    {
      ImGui::BeginGroup();

      ImGui::Text("%s", std::filesystem::absolute(selected_folder.path).string().c_str());

      ImGui::Separator();

      const int32_t columns = 8;

      if (ImGui::BeginChild("Folder View", ImVec2(0, 0), ImGuiChildFlags_NavFlattened,
                            ImGuiWindowFlags_HorizontalScrollbar)) {
        if (ImGui::BeginTable("file table", columns)) {
          for (uint32_t i = 0; i < columns; i++) {
            ImGui::TableSetupColumn(fmt::format("{}", i).c_str());
          }
          for (uint32_t row = 0; row < glm::ceil(selected_folder.files.size() / float(columns));
               row++) {
            ImGui::TableNextRow();
            for (uint32_t col = 0; col < columns; col++) {
              ImGui::TableSetColumnIndex(col);
              if (col + (row * columns) < selected_folder.files.size()) {
                File &file = selected_folder.files[col + (row * columns)];
                if (ImGui::IsItemVisible()) {
                  if (file.file_icon_cache_index == 0) {
                    LoadFileIconImage(file);
                  } else if (false) {
                    file_icon_cache_image_futures[file.file_icon_cache_index].get();
                  }
                }
                DrawFileIcon(file);
              }
            }
          }
        }
        ImGui::EndTable();
      }
      ImGui::EndChild();
      ImGui::EndGroup();
    }
  }
  ImGui::End();
}

void FileExplorer::Destroy() {
  for (uint32_t i = 0; i < file_icon_cache_size; i++) {
    if (file_icon_descriptor_cache[i] != VK_NULL_HANDLE) {
      ImGui_ImplVulkan_RemoveTexture(file_icon_descriptor_cache[i]);
    }
    if (file_icon_image_cache[i].image != VK_NULL_HANDLE) {
      DestroyAllocatedImage(file_icon_image_cache[i]);
    }
  }
}
} // namespace ui
