#include "gltf.h"
#include "types.h"
#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/types.hpp>
#include <fastgltf/util.hpp>
#include <filesystem>
#include <fmt/base.h>
#include <stdlib.h>

void LoadGltf(std::string path) {
  fastgltf::Parser parser;

  std::string full_file_path = gltf_file_path + path + ".gltf";

  auto data = fastgltf::GltfDataBuffer::FromPath(full_file_path);

  if (data.error() != fastgltf::Error::None) {
    fmt::println("[ERROR] gltf model {} failed to load", path);
    abort();
  }

  fastgltf::Options load_options =
      fastgltf::Options::LoadExternalBuffers |
      fastgltf::Options::LoadExternalImages |
      fastgltf::Options::DontRequireValidAssetMember |
      fastgltf::Options::AllowDouble;

  auto asset = parser.loadGltf(data.get(), gltf_file_path, load_options);

  if (auto error = asset.error(); error != fastgltf::Error::None) {
    fmt::println("[ERROR] gltf model {} failed to parse asset", path);
    abort();
  }

  std::vector<uint32_t> indices;
  std::vector<Vertex> vertices;

  for (fastgltf::Mesh &mesh : asset->meshes) {
    for (auto &&p : mesh.primitives) {
      {
        fastgltf::Accessor &index_accessor =
            asset->accessors[p.indicesAccessor.value()];
        indices.reserve(index_accessor.count);
        fmt::println("{}", index_accessor.count);

        fastgltf::iterateAccessor<std::uint32_t>(
            asset.get(), index_accessor,
            [&](std::uint32_t index) { indices.push_back(index); });
      }
    }
  }
}
