#pragma once

#include "types.h"
#include <string>
#include <vector>

static const std::string gltf_file_path = "../assets/models/";

struct MaterialData {
  std::string albedo;
  std::string metal_roughness;
  std::string emissive;
  std::string normal;
  std::string ambient_occlusion;
  glm::vec3 base_color;
};

struct MeshData {
  std::vector<Vertex> vertices;
  std::vector<uint32_t> indices;
  std::vector<glm::mat4> instances;
  std::vector<std::pair<glm::vec3, glm::vec3>> instance_aabbs;
  MaterialData material_data;
};

std::vector<MeshData> LoadModel(std::string path);
