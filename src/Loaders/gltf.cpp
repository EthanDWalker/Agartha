#include "gltf.h"
#include "mesh.h"
#include "types.h"
#include <fastgltf/core.hpp>
#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/util.hpp>
#include <filesystem>
#include <fmt/base.h>
#include <stdlib.h>

MeshData LoadGltf(std::string path) {
  fastgltf::Parser parser;

  std::string full_file_path = gltf_file_path + path + ".gltf";

  auto data = fastgltf::GltfDataBuffer::FromPath(full_file_path);

  if (data.error() != fastgltf::Error::None) {
    fmt::println("[ERROR] gltf model {} failed to load", path);
    abort();
  }

  fastgltf::Options load_options =
      fastgltf::Options::LoadExternalBuffers |
      fastgltf::Options::DontRequireValidAssetMember;

  auto asset = parser.loadGltf(data.get(), gltf_file_path, load_options);

  if (asset.error() != fastgltf::Error::None) {
    fmt::println("[ERROR] gltf model {} failed to parse asset", path);
    abort();
  }

  std::vector<uint32_t> indices;
  std::vector<Vertex> vertices;

  for (fastgltf::Mesh &mesh : asset->meshes) {
    for (auto &&p : mesh.primitives) {
      {
        auto &index_accessor = asset->accessors[p.indicesAccessor.value()];
        indices.reserve(index_accessor.count);

        fastgltf::iterateAccessor<std::uint32_t>(
            asset.get(), index_accessor,
            [&](std::uint32_t index) { indices.push_back(index); });
      }

      {
        auto &position_accessor =
            asset->accessors[p.findAttribute("POSITION")->accessorIndex];
        vertices.resize(position_accessor.count);

        fastgltf::iterateAccessorWithIndex<glm::vec3>(
            asset.get(), position_accessor,
            [&](glm::vec3 position, size_t index) {
              Vertex vertex;
              vertex.position = position;
              vertex.normal = {1, 0, 0};
              vertex.color = glm::vec4{1.f};
              vertex.uv_x = 0;
              vertex.uv_y = 0;
              vertices[index] = vertex;
            });
      }

      auto normals = p.findAttribute("NORMAL");
      if (normals != p.attributes.end()) {
        fastgltf::iterateAccessorWithIndex<glm::vec3>(
            asset.get(), asset->accessors[normals->accessorIndex],
            [&](glm::vec3 normal, std::size_t index) {
              vertices[index].normal = normal;
            });
      }

      auto uv = p.findAttribute("TEXCOORD_0");
      if (uv != p.attributes.end()) {
        fastgltf::iterateAccessorWithIndex<glm::vec2>(
            asset.get(), asset->accessors[uv->accessorIndex],
            [&](glm::vec2 uv, std::size_t index) {
              vertices[index].uv_x = uv.x;
              vertices[index].uv_y = uv.y;
            });
      }

      auto color = p.findAttribute("COLOR_0");
      if (color != p.attributes.end()) {
        fastgltf::iterateAccessorWithIndex<glm::vec4>(
            asset.get(), asset->accessors[color->accessorIndex],
            [&](glm::vec4 color, std::size_t index) {
              vertices[index].color = color;
            });
      }
    }
  }

  return MeshData{
      vertices,
      indices,
  };
}
