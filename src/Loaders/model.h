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
};

struct MeshData {
  std::vector<Vertex> vertices;
  std::vector<uint32_t> indices;
  std::vector<glm::mat4> instances;
  glm::vec3 collilder_min;
  glm::vec3 collilder_max;
  MaterialData material_data;
};

std::vector<MeshData> LoadModel(std::string path);
