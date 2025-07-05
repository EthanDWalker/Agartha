#pragma once

#include "Backend/allocated_image.h"
#include <GLFW/glfw3.h>
#include <glm/vec3.hpp>
#include <volk.h>

void UpdateUi(GLFWwindow *window, glm::vec3 *directional_light);

void RenderUi(VkCommandBuffer cmd, AllocatedImage &draw_image);
