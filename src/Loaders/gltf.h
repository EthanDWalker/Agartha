#pragma once

#include "mesh.h"

#include <string>

static const std::string gltf_file_path = "../assets/models/";

MeshData LoadGltf(std::string path);
