#include "context.h"
#define VOLK_IMPLEMENTATION
#include "Backend/util.h"
#include <GLFW/glfw3.h>
#include <VkBootstrap.h>
#include <cassert>
#include <volk.h>

void InitVulkanContext(GLFWwindow *window, bool debug, VulkanContext &context) {
  volkInitialize();
  vkb::InstanceBuilder instance_builder;

#if !defined(NDEBUG)
  fmt::println("DEBUG ACTIVE");
#endif
  auto instance_return = instance_builder
                             .set_app_name("Engine")
#if !defined(NDEBUG)
                             .request_validation_layers()
                             .use_default_debug_messenger()
#endif
                             .require_api_version(1, 3)
                             .build();

  assert(instance_return);

  vkb::Instance vkb_instance = instance_return.value();

  context.instance = vkb_instance.instance;
  volkLoadInstance(context.instance);
  context.debug_messenger = vkb_instance.debug_messenger;

  VK_CHECK(glfwCreateWindowSurface(context.instance, window, nullptr,
                                   &context.surface));

  VkPhysicalDeviceFeatures features{};
  features.geometryShader = true;
  features.multiDrawIndirect = true;
  features.fragmentStoresAndAtomics = true;
  features.shaderFloat64 = true;

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
  features_12.descriptorBindingUniformBufferUpdateAfterBind = true;
  features_12.descriptorBindingSampledImageUpdateAfterBind = true;
  features_12.descriptorBindingStorageBufferUpdateAfterBind = true;
  features_12.descriptorBindingStorageImageUpdateAfterBind = true;

  VkPhysicalDeviceRobustness2FeaturesEXT robustness2{};
  robustness2.sType =
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT;
  robustness2.pNext = nullptr;
  robustness2.nullDescriptor = true;

  VkPhysicalDeviceAccelerationStructureFeaturesKHR as_features{};
  as_features.sType =
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
  as_features.accelerationStructure = true;
  as_features.descriptorBindingAccelerationStructureUpdateAfterBind = true;

  VkPhysicalDeviceRayTracingPipelineFeaturesKHR raytracing_features{};
  raytracing_features.sType =
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
  raytracing_features.rayTracingPipeline = true;

  VkPhysicalDeviceRayTracingPositionFetchFeaturesKHR
      raytracing_position_features{};
  raytracing_position_features.sType =
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_POSITION_FETCH_FEATURES_KHR;
  raytracing_position_features.rayTracingPositionFetch = true;

  vkb::PhysicalDeviceSelector physical_device_selector{vkb_instance};
  vkb::PhysicalDevice vkb_physical_device =
      physical_device_selector.set_minimum_version(1, 3)
          .set_required_features_13(features_13)
          .set_required_features_12(features_12)
          .set_required_features(features)
          .set_surface(context.surface)
          .add_required_extension("VK_EXT_robustness2")
          .add_required_extension_features(robustness2)
          .add_required_extension(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME)
          .add_required_extension_features(as_features)
          .add_required_extension(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME)
          .add_required_extension_features(raytracing_features)
          .add_required_extension(
              VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME)
          .select()
          .value();
  context.physical_device = vkb_physical_device.physical_device;

  vkb::DeviceBuilder device_builder{vkb_physical_device};

  vkb::Device vkb_device = device_builder.build().value();
  context.device = vkb_device.device;
  volkLoadDevice(context.device);

  context.graphics_queue =
      vkb_device.get_queue(vkb::QueueType::graphics).value();
  context.graphics_queue_index =
      vkb_device.get_queue_index(vkb::QueueType::graphics).value();

  context.compute_queue = vkb_device.get_queue(vkb::QueueType::compute).value();
  context.compute_queue_index =
      vkb_device.get_queue_index(vkb::QueueType::compute).value();

  VmaVulkanFunctions vulkan_functions{};
  vulkan_functions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
  vulkan_functions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;

  VmaAllocatorCreateInfo allocator_ci{};
  allocator_ci.device = context.device;
  allocator_ci.instance = context.instance;
  allocator_ci.physicalDevice = context.physical_device;
  allocator_ci.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
  allocator_ci.pVulkanFunctions = &vulkan_functions;
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
