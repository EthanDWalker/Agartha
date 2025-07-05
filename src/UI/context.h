#pragma once

#include "Backend/context.h"

void CreateUiContext(VulkanContext &vulkan_context, GLFWwindow *window,
                     VkFormat *color_format);

void DestroyUiContext();
