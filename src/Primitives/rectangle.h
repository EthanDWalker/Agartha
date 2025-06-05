#pragma once
#include "types.h"
#include <cstdint>
#include <vector>

static const std::vector<Vertex> rectangle_vertices = {
    Vertex{{-0.5f, -0.5f, 0.5f}, 0.0f, {0, 0, 1}, 0.0f},
    Vertex{{0.5f, -0.5f, 0.5f}, 1.0f, {0, 0, 1}, 0.0f},
    Vertex{{0.5f, 0.5f, 0.5f}, 1.0f, {0, 0, 1}, 1.0f},
    Vertex{{-0.5f, 0.5f, 0.5f}, 0.0f, {0, 0, 1}, 1.0f},
};

static const std::vector<uint32_t> rectangle_indices = {
    0, 1, 2, 2, 3, 0,
};
