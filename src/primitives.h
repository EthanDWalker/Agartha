#pragma once
#include "Parsers/model.h"

struct Primitives {
  static SceneNodeData GetCubeData() {
    return ParseModel("C:/Users/ethan/Developer/Agartha/assets/models/cube.gltf").front();
  }
};
