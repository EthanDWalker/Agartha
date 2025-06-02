#pragma once
#include "types.h"
#include <cstdint>
#include <vector>

static const std::vector<Vertex> cube_vertices = {
    // Front face (+Z)
    Vertex{{-0.5f, -0.5f, 0.5f}, 0.0f, {0, 0, 1}, 0.0f},
    Vertex{{0.5f, -0.5f, 0.5f}, 1.0f, {0, 0, 1}, 0.0f},
    Vertex{{0.5f, 0.5f, 0.5f}, 1.0f, {0, 0, 1}, 1.0f},
    Vertex{{-0.5f, 0.5f, 0.5f}, 0.0f, {0, 0, 1}, 1.0f},

    // Back face (-Z)
    Vertex{{0.5f, -0.5f, -0.5f}, 0.0f, {0, 0, -1}, 0.0f},
    Vertex{{-0.5f, -0.5f, -0.5f}, 1.0f, {0, 0, -1}, 0.0f},
    Vertex{{-0.5f, 0.5f, -0.5f}, 1.0f, {0, 0, -1}, 1.0f},
    Vertex{{0.5f, 0.5f, -0.5f}, 0.0f, {0, 0, -1}, 1.0f},

    // Left face (-X)
    Vertex{{-0.5f, -0.5f, -0.5f}, 0.0f, {-1, 0, 0}, 0.0f},
    Vertex{{-0.5f, -0.5f, 0.5f}, 1.0f, {-1, 0, 0}, 0.0f},
    Vertex{{-0.5f, 0.5f, 0.5f}, 1.0f, {-1, 0, 0}, 1.0f},
    Vertex{{-0.5f, 0.5f, -0.5f}, 0.0f, {-1, 0, 0}, 1.0f},

    // Right face (+X)
    Vertex{{0.5f, -0.5f, 0.5f}, 0.0f, {1, 0, 0}, 0.0f},
    Vertex{{0.5f, -0.5f, -0.5f}, 1.0f, {1, 0, 0}, 0.0f},
    Vertex{{0.5f, 0.5f, -0.5f}, 1.0f, {1, 0, 0}, 1.0f},
    Vertex{{0.5f, 0.5f, 0.5f}, 0.0f, {1, 0, 0}, 1.0f},

    // Top face (+Y)
    Vertex{{-0.5f, 0.5f, 0.5f}, 0.0f, {0, 1, 0}, 0.0f},
    Vertex{{0.5f, 0.5f, 0.5f}, 1.0f, {0, 1, 0}, 0.0f},
    Vertex{{0.5f, 0.5f, -0.5f}, 1.0f, {0, 1, 0}, 1.0f},
    Vertex{{-0.5f, 0.5f, -0.5f}, 0.0f, {0, 1, 0}, 1.0f},

    // Bottom face (-Y)
    Vertex{{-0.5f, -0.5f, -0.5f}, 0.0f, {0, -1, 0}, 0.0f},
    Vertex{{0.5f, -0.5f, -0.5f}, 1.0f, {0, -1, 0}, 0.0f},
    Vertex{{0.5f, -0.5f, 0.5f}, 1.0f, {0, -1, 0}, 1.0f},
    Vertex{{-0.5f, -0.5f, 0.5f}, 0.0f, {0, -1, 0}, 1.0f},
};

static const std::vector<uint32_t> cube_indices = {
    // Front
    0, 1, 2, 2, 3, 0,
    // Back
    4, 5, 6, 6, 7, 4,
    // Left
    8, 9, 10, 10, 11, 8,
    // Right
    12, 13, 14, 14, 15, 12,
    // Top
    16, 17, 18, 18, 19, 16,
    // Bottom
    20, 21, 22, 22, 23, 20};
