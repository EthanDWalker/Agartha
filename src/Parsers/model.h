#pragma once

#include "types.h"
#include <glm/vec3.hpp>
#include <span>
#include <string>
#include <vector>

struct MaterialData {
  std::string albedo;
  std::string metal_roughness;
  std::string emissive;
  std::string normal;
  std::string ambient_occlusion;
};

struct PrimitiveData {
  MaterialData material_data;
  std::vector<Vertex> vertices;
  std::vector<uint32_t> indices;
  std::vector<glm::mat4> instances;
  AabbBounds aabb_bounds;
  SphereBounds sphere_bounds;
};

struct MeshData {
  std::vector<PrimitiveData> primitives;
};

struct SceneNodeData {
  MeshData mesh_data;
  std::string name;
  std::vector<SceneNodeData> children;
};

void GetMeshBounds(std::span<Vertex> vertices, SphereBounds &sphere_bounds,
                   AabbBounds &aabb_bounds);

std::vector<SceneNodeData> ParseModel(std::string path);
