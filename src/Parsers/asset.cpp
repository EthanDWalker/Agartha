#include "asset.h"
#include "Backend/buffer.h"
#include "Backend/context.h"
#include "Backend/immediate_submit.h"
#include "Parsers/model.h"
#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fmt/base.h>
#include <fstream>

struct AssetLut {
  size_t vertex_offset;
  size_t index_offset;
  size_t material_data_offset;
};

AssetData ParseAsset(std::filesystem::path file_path) {
  std::ifstream file(file_path.string(), std::ios::ate | std::ios::binary);

  if (!file.is_open()) {
    return {};
  }

  size_t file_size = (size_t)file.tellg();

  std::vector<char> buffer(file_size);

  file.seekg(0);

  file.read((char *)buffer.data(), file_size);

  file.close();

  AssetData asset_data{};

  AssetLut asset_lut = *(AssetLut *)buffer.data();

  fmt::println("{}", *(size_t *)&buffer[asset_lut.vertex_offset]);
  fmt::println("{}", *(size_t *)&buffer[asset_lut.index_offset]);

  asset_data.vertices.resize(*(size_t *)&buffer[asset_lut.vertex_offset]);
  asset_data.indices.resize(*(size_t *)&buffer[asset_lut.index_offset]);

  memcpy(asset_data.vertices.data(), &buffer[asset_lut.vertex_offset + sizeof(size_t)],
         asset_data.vertices.size() * sizeof(Vertex));
  memcpy(asset_data.indices.data(), &buffer[asset_lut.index_offset + sizeof(size_t)],
         asset_data.indices.size() * sizeof(uint32_t));

  std::array<std::string, 5> materials;
  uint8_t material_index = 0;
  for (size_t i = asset_lut.material_data_offset; i < buffer.size(); i++) {
    materials[material_index] += buffer[i];
    if (buffer[i] == '\0') {
      material_index++;
      if (material_index >= materials.size()) {
        break;
      }
    }
  }

  asset_data.material_data.albedo =
      materials[0].size() == 1 ? "" : materials[0];
  asset_data.material_data.metal_roughness =
      materials[1].size() == 1 ? "" : materials[1];
  asset_data.material_data.emissive =
      materials[2].size() == 1 ? "" : materials[2];
  asset_data.material_data.normal =
      materials[3].size() == 1 ? "" : materials[3];
  asset_data.material_data.ambient_occlusion =
      materials[4].size() == 1 ? "" : materials[4];

  return asset_data;
}

void SerializeAsset(AllocatedBuffer index_buffer,
                    Mesh &mesh, MaterialData &material_data,
                    std::filesystem::path file_path) {
  VkDeviceSize vertex_size = mesh.vertex_buffer.info.size;
  VkDeviceSize index_size = sizeof(uint32_t) * mesh.index_count;

  AllocatedBuffer mesh_buffer;
  CreateBuffer(vertex_size + index_size,
               VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_TO_CPU,
               mesh_buffer);

  ImmediateSubmit::Submit([&](VkCommandBuffer cmd) {
    {
      VkBufferCopy2 copy_region{};
      copy_region.sType = VK_STRUCTURE_TYPE_BUFFER_COPY_2;
      copy_region.size = vertex_size;
      copy_region.srcOffset = 0;
      copy_region.dstOffset = 0;

      VkCopyBufferInfo2 buffer_copy{};
      buffer_copy.sType = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2;
      buffer_copy.srcBuffer = mesh.vertex_buffer.buffer;
      buffer_copy.dstBuffer = mesh_buffer.buffer;
      buffer_copy.pRegions = &copy_region;
      buffer_copy.regionCount = 1;

      vkCmdCopyBuffer2(cmd, &buffer_copy);
    }

    {
      VkBufferCopy2 copy_region{};
      copy_region.sType = VK_STRUCTURE_TYPE_BUFFER_COPY_2;
      copy_region.size = index_size;
      copy_region.srcOffset = sizeof(uint32_t) * mesh.first_index;
      copy_region.dstOffset = vertex_size;

      VkCopyBufferInfo2 buffer_copy{};
      buffer_copy.sType = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2;
      buffer_copy.srcBuffer = index_buffer.buffer;
      buffer_copy.dstBuffer = mesh_buffer.buffer;
      buffer_copy.pRegions = &copy_region;
      buffer_copy.regionCount = 1;

      vkCmdCopyBuffer2(cmd, &buffer_copy);
    }
  });

  std::ofstream file(file_path.string(),
                     std::ios::ate | std::ios::out | std::ios::binary);

  if (!file) {
    fmt::println("[ERROR] failed to open file {}... trying again",
                 file_path.string());
    file.open(file_path.string(),
              std::ios::ate | std::ios::out | std::ios::binary);
    if (!file) {
      fmt::println("[ERROR] failed to open file again {}... returning",
                   file_path.string());
      return;
    }
  }

  AssetLut lut{};
  lut.vertex_offset = sizeof(AssetLut);
  lut.index_offset = lut.vertex_offset + vertex_size + sizeof(size_t);
  lut.material_data_offset = lut.index_offset + index_size + sizeof(size_t);

  file.write(reinterpret_cast<const char *>(&lut), sizeof(AssetLut));

  size_t vertex_count = vertex_size / sizeof(Vertex);
  file.write((const char *)&vertex_count, sizeof(size_t));
  file.write(reinterpret_cast<const char *>(mesh_buffer.info.pMappedData),
             vertex_size);

  size_t index_count = index_size / sizeof(uint32_t);
  file.write((const char *)&index_count, sizeof(size_t));
  file.write(reinterpret_cast<const char *>(mesh_buffer.info.pMappedData) +
                 vertex_size,
             index_size);

  file.write(material_data.albedo.data(), material_data.albedo.size() + 1);
  file.write(material_data.metal_roughness.data(),
             material_data.metal_roughness.size() + 1);
  file.write(material_data.emissive.data(), material_data.emissive.size() + 1);
  file.write(material_data.normal.data(), material_data.normal.size() + 1);
  file.write(material_data.ambient_occlusion.data(),
             material_data.ambient_occlusion.size() + 1);

  file.close();

  DestroyBuffer(mesh_buffer);
}
