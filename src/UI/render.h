#pragma once

#include <GLFW/glfw3.h>
#include <glm/vec3.hpp>
#include <volk.h>

bool UpdateUi(GLFWwindow *window, glm::vec3 *directional_light);

void RenderUi(VkCommandBuffer cmd);
