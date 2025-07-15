#pragma once

#include "Backend/buffer.h"
#include "Backend/context.h"
#include "Parsers/model.h"
#include <filesystem>

struct AssetData {
  MaterialData material_data;
  std::vector<Vertex> vertices;
  std::vector<uint32_t> indices;
};

AssetData ParseAsset(std::filesystem::path file_path);

void SerializeAsset(VulkanContext &vulkan_context, AllocatedBuffer index_buffer,
                    Mesh &mesh, MaterialData &material_data,
                    std::filesystem::path file_path);
