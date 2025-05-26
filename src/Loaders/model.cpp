#include "model.h"
#include "fastgltf/types.hpp"
#include "timer.h"
#include "types.h"
#include <fastgltf/core.hpp>
#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/util.hpp>
#include <filesystem>
#include <fmt/base.h>
#include <string>
#include <variant>
#include <vector>

MaterialData ParseMaterialData(fastgltf::Material &material,
                               std::span<size_t> textures,
                               std::span<std::string> images) {

  MaterialData new_material;

  new_material.albedo =
      material.pbrData.baseColorTexture.has_value()
          ? images[textures[material.pbrData.baseColorTexture->textureIndex]]
          : "";
  new_material.ambient_occlusion =
      material.occlusionTexture.has_value()
          ? images[textures[material.occlusionTexture->textureIndex]]
          : "";
  new_material.normal =
      material.normalTexture.has_value()
          ? images[textures[material.normalTexture->textureIndex]]
          : "";
  new_material.emissive =
      material.emissiveTexture.has_value()
          ? images[textures[material.emissiveTexture->textureIndex]]
          : "";
  new_material.metal_roughness =
      material.pbrData.metallicRoughnessTexture.has_value()
          ? images[textures[material.pbrData.metallicRoughnessTexture
                                ->textureIndex]]
          : "";
  return new_material;
}

std::vector<MeshData> LoadModel(std::string path) {
  std::string full_file_path = gltf_file_path + path;

  std::filesystem::path file_path = full_file_path;

  fastgltf::Parser parser;

  auto data = fastgltf::GltfDataBuffer::FromPath(full_file_path);

  if (data.error() != fastgltf::Error::None) {
    fmt::println("[ERROR] gltf model {} failed to load", path);
    abort();
  }

  fastgltf::Options load_options =
      fastgltf::Options::LoadExternalBuffers |
      fastgltf::Options::DontRequireValidAssetMember;

  auto asset =
      parser.loadGltf(data.get(), file_path.parent_path(), load_options);

  if (asset.error() != fastgltf::Error::None) {
    fmt::println("[ERROR] gltf model {} failed to parse asset", path);
    abort();
  }

  std::vector<std::string> images;
  images.reserve(asset->images.size());

  for (fastgltf::Image &image : asset->images) {
    std::visit(fastgltf::visitor{
                   [](auto &arg) {},
                   [&](fastgltf::sources::URI &filePath) {
                     assert(filePath.fileByteOffset == 0);
                     assert(filePath.uri.isLocalPath());

                     const std::string path(filePath.uri.path().begin(),
                                            filePath.uri.path().end());

                     images.push_back(path);
                   },
               },
               image.data);
  }

  std::vector<size_t> textures;
  textures.reserve(asset->textures.size());
  for (fastgltf::Texture texture : asset->textures) {
    textures.push_back(texture.imageIndex.value());
  }

  std::vector<MaterialData> material_datas;
  material_datas.reserve(asset->materials.size());

  for (fastgltf::Material &material : asset->materials) {
    material_datas.push_back(ParseMaterialData(material, textures, images));
  }

  std::vector<uint32_t> indices;
  std::vector<Vertex> vertices;

  std::vector<MeshData> mesh_data;

  for (fastgltf::Mesh &mesh : asset->meshes) {

    mesh_data.reserve(mesh.primitives.size());

    for (auto &&p : mesh.primitives) {
      indices.clear();
      vertices.clear();

      auto &index_accessor = asset->accessors[p.indicesAccessor.value()];
      indices.reserve(indices.size() + index_accessor.count);

      fastgltf::iterateAccessor<std::uint32_t>(asset.get(), index_accessor,
                                               [&](std::uint32_t index) {
                                                 indices.push_back(index);
                                                 ;
                                               });

      auto &position_accessor =
          asset->accessors[p.findAttribute("POSITION")->accessorIndex];

      vertices.resize(vertices.size() + position_accessor.count);

      fastgltf::iterateAccessorWithIndex<glm::vec3>(
          asset.get(), position_accessor,
          [&](glm::vec3 position, size_t index) {
            Vertex vertex;
            vertex.position = position / 30.f;
            vertex.normal = {1, 0, 0};
            vertex.color = glm::vec4{1.f};
            vertex.uv_x = 0;
            vertex.uv_y = 0;
            vertices[index] = vertex;
          });

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

      MaterialData material_data{};
      if (p.materialIndex.has_value()) {
        material_data = material_datas[p.materialIndex.value()];
      }

      mesh_data.push_back({
          .vertices = vertices,
          .indices = indices,
          .material_data = material_data,
      });
    }
  }

  return mesh_data;
}
