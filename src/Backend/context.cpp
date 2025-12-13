#include "context.h"
#define VOLK_IMPLEMENTATION
#include "Backend/util.h"
#include <GLFW/glfw3.h>
#include <VkBootstrap.h>
#include <cassert>
#include <volk.h>

std::mutex VulkanContext::graphics_queue_mutex = {};
std::mutex VulkanContext::compute_queue_mutex = {};
VkInstance VulkanContext::instance = VK_NULL_HANDLE;
VkDevice VulkanContext::device = VK_NULL_HANDLE;
VkPhysicalDevice VulkanContext::physical_device = VK_NULL_HANDLE;
VkSurfaceKHR VulkanContext::surface = VK_NULL_HANDLE;
VmaAllocator VulkanContext::allocator = VK_NULL_HANDLE;
VkDebugUtilsMessengerEXT VulkanContext::debug_messenger = VK_NULL_HANDLE;
VkQueue VulkanContext::graphics_queue = VK_NULL_HANDLE;
VkQueue VulkanContext::compute_queue = VK_NULL_HANDLE;
uint32_t VulkanContext::graphics_queue_index = {};
uint32_t VulkanContext::compute_queue_index = {};

void VulkanContext::Init(GLFWwindow *window) {
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

  instance = vkb_instance.instance;
  volkLoadInstance(instance);
  debug_messenger = vkb_instance.debug_messenger;

  VK_CHECK(glfwCreateWindowSurface(instance, window, nullptr, &surface));

  VkPhysicalDeviceFeatures features{};
  features.geometryShader = true;
  features.multiDrawIndirect = true;
  features.fragmentStoresAndAtomics = true;
  features.fillModeNonSolid = true;

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
  robustness2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT;
  robustness2.pNext = nullptr;
  robustness2.nullDescriptor = true;

  VkPhysicalDeviceAccelerationStructureFeaturesKHR as_features{};
  as_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
  as_features.accelerationStructure = true;
  as_features.descriptorBindingAccelerationStructureUpdateAfterBind = true;

  VkPhysicalDeviceRayTracingPipelineFeaturesKHR raytracing_features{};
  raytracing_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
  raytracing_features.rayTracingPipeline = true;

  vkb::PhysicalDeviceSelector physical_device_selector{vkb_instance};

  vkb::PhysicalDevice vkb_physical_device =
      physical_device_selector.set_minimum_version(1, 3)
          .set_required_features_13(features_13)
          .set_required_features_12(features_12)
          .set_required_features(features)
          .set_surface(surface)
          .add_required_extension(VK_EXT_ROBUSTNESS_2_EXTENSION_NAME)
          .add_required_extension_features(robustness2)
          .add_required_extension(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME)
          .add_required_extension_features(as_features)
          .add_required_extension(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME)
          .add_required_extension_features(raytracing_features)
          .add_required_extension(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME)
          .select()
          .value();
  physical_device = vkb_physical_device.physical_device;

  vkb::DeviceBuilder device_builder{vkb_physical_device};

  vkb::Device vkb_device = device_builder.build().value();
  device = vkb_device.device;
  volkLoadDevice(device);

  graphics_queue = vkb_device.get_queue(vkb::QueueType::graphics).value();
  graphics_queue_index = vkb_device.get_queue_index(vkb::QueueType::graphics).value();

  compute_queue = vkb_device.get_queue(vkb::QueueType::compute).value();
  compute_queue_index = vkb_device.get_queue_index(vkb::QueueType::compute).value();

  VmaVulkanFunctions vulkan_functions{};
  vulkan_functions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
  vulkan_functions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;

  VmaAllocatorCreateInfo allocator_ci{};
  allocator_ci.device = device;
  allocator_ci.instance = instance;
  allocator_ci.physicalDevice = physical_device;
  allocator_ci.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
  allocator_ci.pVulkanFunctions = &vulkan_functions;
  VK_CHECK(vmaCreateAllocator(&allocator_ci, &allocator));
}

void VulkanContext::Destroy() {
  vkDeviceWaitIdle(device);
  vkDestroySurfaceKHR(instance, surface, nullptr);
  vmaDestroyAllocator(allocator);
  vkDestroyDevice(device, nullptr);
  vkb::destroy_debug_utils_messenger(instance, debug_messenger);
  vkDestroyInstance(instance, nullptr);
}
