// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/vulkan/vulkan_implementation_android.h"

#include "base/files/file_path.h"
#include "gpu/vulkan/vulkan_function_pointers.h"
#include "gpu/vulkan/vulkan_instance.h"
#include "gpu/vulkan/vulkan_surface.h"

namespace gpu {

VulkanImplementationAndroid::VulkanImplementationAndroid() {}

VulkanImplementationAndroid::~VulkanImplementationAndroid() {}

bool VulkanImplementationAndroid::InitializeVulkanInstance() {
  std::vector<const char*> required_extensions;
  required_extensions.push_back(VK_KHR_ANDROID_SURFACE_EXTENSION_NAME);

  VulkanFunctionPointers* vulkan_function_pointers =
      gpu::GetVulkanFunctionPointers();

  base::NativeLibraryLoadError native_library_load_error;
  vulkan_function_pointers->vulkan_loader_library_ = base::LoadNativeLibrary(
      base::FilePath("libvulkan.so"), &native_library_load_error);
  if (!vulkan_function_pointers->vulkan_loader_library_)
    return false;

  if (!vulkan_instance_.Initialize(required_extensions)) {
    vulkan_instance_.Destroy();
    return false;
  }

  // Initialize platform function pointers
  vkCreateAndroidSurfaceKHR_ = reinterpret_cast<PFN_vkCreateAndroidSurfaceKHR>(
      vulkan_function_pointers->vkGetInstanceProcAddr(
          vulkan_instance_.vk_instance(), "vkCreateAndroidSurfaceKHR"));
  if (!vkCreateAndroidSurfaceKHR_) {
    vulkan_instance_.Destroy();
    return false;
  }

  return true;
}

VkInstance VulkanImplementationAndroid::GetVulkanInstance() {
  return vulkan_instance_.vk_instance();
}

std::unique_ptr<VulkanSurface> VulkanImplementationAndroid::CreateViewSurface(
    gfx::AcceleratedWidget window) {
  VulkanFunctionPointers* vulkan_function_pointers =
      gpu::GetVulkanFunctionPointers();

  VkSurfaceKHR surface;
  VkAndroidSurfaceCreateInfoKHR surface_create_info = {};
  surface_create_info.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
  surface_create_info.window = window;
  result = vulkan_function_pointers->vkCreateAndroidSurfaceKHR(
      GetVulkanInstance(), &surface_create_info, nullptr, &surface);
  if (VK_SUCCESS != result) {
    DLOG(ERROR) << "vkCreateAndroidSurfaceKHR() failed: " << result;
    return nullptr;
  }

  return std::make_unique<VulkanSurface>(GetVulkanInstance(), surface);
}

bool VulkanImplementationAndroid::GetPhysicalDevicePresentationSupport(
    VkPhysicalDevice device,
    const std::vector<VkQueueFamilyProperties>& queue_family_properties,
    uint32_t queue_family_index) override {
  // On Android, all physical devices and queue families must be capable of
  // presentation with any native window.
  // As a result there is no Android-specific query for these capabilities.
  return true;
}

}  // namespace gpu
