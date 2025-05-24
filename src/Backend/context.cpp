#include "context.h"
#include "Backend/util.h"
#include <GLFW/glfw3.h>
#include <VkBootstrap.h>
#include <cassert>
#include <vma/vk_mem_alloc.h>
#include <vulkan/vulkan.h>

void InitVulkanContext(GLFWwindow *window, bool debug, VulkanContext &context) {
  vkb::InstanceBuilder instance_builder;
  auto instance_return = instance_builder.set_app_name("Engine")
                             .request_validation_layers()
                             .use_default_debug_messenger()
                             .require_api_version(1, 3)
                             .build();

  assert(instance_return);

  vkb::Instance vkb_instance = instance_return.value();

  context.instance = vkb_instance.instance;
  context.debug_messenger = vkb_instance.debug_messenger;

  VK_CHECK(glfwCreateWindowSurface(context.instance, window, nullptr,
                                   &context.surface));

  VkPhysicalDeviceVulkan14Features features_14{};
  features_14.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES;

  VkPhysicalDeviceVulkan13Features features_13{};
  features_13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
  features_13.dynamicRendering = true;
  features_13.synchronization2 = true;

  VkPhysicalDeviceVulkan12Features features_12{};
  features_12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
  features_12.bufferDeviceAddress = true;
  features_12.descriptorIndexing = true;
  features_12.shaderSampledImageArrayNonUniformIndexing = true;
  features_12.runtimeDescriptorArray = true;
  features_12.descriptorBindingVariableDescriptorCount = true;
  features_12.descriptorBindingPartiallyBound = true;

  VkPhysicalDeviceVulkan11Features features_11;
  features_11.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;

  VkPhysicalDeviceRobustness2FeaturesEXT robustness2 = {};
  robustness2.sType =
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT;
  robustness2.pNext = nullptr;
  robustness2.nullDescriptor = VK_TRUE;

  vkb::PhysicalDeviceSelector physical_device_selector{vkb_instance};
  vkb::PhysicalDevice vkb_physical_device =
      physical_device_selector.set_minimum_version(1, 3)
          .set_required_features_13(features_13)
          .set_required_features_12(features_12)
          .set_surface(context.surface)
          .add_required_extension("VK_EXT_robustness2")
          .add_required_extension_features(robustness2)
          .select()
          .value();
  context.physical_device = vkb_physical_device.physical_device;

  vkb::DeviceBuilder device_builder{vkb_physical_device};

  vkb::Device vkb_device = device_builder.build().value();
  context.device = vkb_device.device;

  context.graphics_queue =
      vkb_device.get_queue(vkb::QueueType::graphics).value();
  context.graphics_queue_index =
      vkb_device.get_queue_index(vkb::QueueType::graphics).value();

  VmaAllocatorCreateInfo allocator_ci{};
  allocator_ci.device = context.device;
  allocator_ci.instance = context.instance;
  allocator_ci.physicalDevice = context.physical_device;
  allocator_ci.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
  VK_CHECK(vmaCreateAllocator(&allocator_ci, &context.allocator));
}

void DestroyVulkanContext(VulkanContext &context) {
  vkDeviceWaitIdle(context.device);
  vkDestroySurfaceKHR(context.instance, context.surface, nullptr);
  vmaDestroyAllocator(context.allocator);
  vkDestroyDevice(context.device, nullptr);
  vkb::destroy_debug_utils_messenger(context.instance, context.debug_messenger);
  vkDestroyInstance(context.instance, nullptr);
}
