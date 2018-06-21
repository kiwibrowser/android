#!/usr/bin/env python
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""code generator for Vulkan function pointers."""

import optparse
import os
import platform
import sys
from subprocess import call

VULKAN_DEVICE_FUNCTIONS = [
{ 'name': 'vkAcquireNextImageKHR' },
{ 'name': 'vkAllocateCommandBuffers' },
{ 'name': 'vkAllocateDescriptorSets' },
{ 'name': 'vkCreateCommandPool' },
{ 'name': 'vkCreateDescriptorPool' },
{ 'name': 'vkCreateDescriptorSetLayout' },
{ 'name': 'vkCreateFence' },
{ 'name': 'vkCreateFramebuffer' },
{ 'name': 'vkCreateImageView' },
{ 'name': 'vkCreateRenderPass' },
{ 'name': 'vkCreateSampler' },
{ 'name': 'vkCreateSemaphore' },
{ 'name': 'vkCreateShaderModule' },
{ 'name': 'vkCreateSwapchainKHR' },
{ 'name': 'vkDestroyCommandPool' },
{ 'name': 'vkDestroyDescriptorPool' },
{ 'name': 'vkDestroyDescriptorSetLayout' },
{ 'name': 'vkDestroyDevice' },
{ 'name': 'vkDestroyFramebuffer' },
{ 'name': 'vkDestroyFence' },
{ 'name': 'vkDestroyImageView' },
{ 'name': 'vkDestroyRenderPass' },
{ 'name': 'vkDestroySampler' },
{ 'name': 'vkDestroySemaphore' },
{ 'name': 'vkDestroyShaderModule' },
{ 'name': 'vkDestroySwapchainKHR' },
{ 'name': 'vkFreeCommandBuffers' },
{ 'name': 'vkFreeDescriptorSets' },
{ 'name': 'vkGetDeviceQueue' },
{ 'name': 'vkGetFenceStatus' },
{ 'name': 'vkGetSwapchainImagesKHR' },
{ 'name': 'vkResetFences' },
{ 'name': 'vkUpdateDescriptorSets' },
{ 'name': 'vkWaitForFences' },
]

VULKAN_QUEUE_FUNCTIONS = [
{ 'name': 'vkQueuePresentKHR' },
{ 'name': 'vkQueueSubmit' },
{ 'name': 'vkQueueWaitIdle' },
]

VULKAN_COMMAND_BUFFER_FUNCTIONS = [
{ 'name': 'vkBeginCommandBuffer' },
{ 'name': 'vkCmdBeginRenderPass' },
{ 'name': 'vkCmdEndRenderPass' },
{ 'name': 'vkCmdExecuteCommands' },
{ 'name': 'vkCmdNextSubpass' },
{ 'name': 'vkCmdPipelineBarrier' },
{ 'name': 'vkEndCommandBuffer' },
{ 'name': 'vkResetCommandBuffer' },
]

SELF_LOCATION = os.path.dirname(os.path.abspath(__file__))

LICENSE_AND_HEADER = """\
// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file is auto-generated from
// gpu/vulkan/generate_bindings.py
// It's formatted by clang-format using chromium coding style:
//    clang-format -i -style=chromium filename
// DO NOT EDIT!

"""

def GenerateHeaderFile(file, device_functions, queue_functions,
                       command_buffer_functions):
  """Generates gpu/vulkan/vulkan_function_pointers.h"""

  file.write(LICENSE_AND_HEADER +
"""

#ifndef GPU_VULKAN_VULKAN_FUNCTION_POINTERS_H_
#define GPU_VULKAN_VULKAN_FUNCTION_POINTERS_H_

#include <vulkan/vulkan.h>

#include "base/native_library.h"
#include "gpu/vulkan/vulkan_export.h"

namespace gpu {

struct VulkanFunctionPointers;

VULKAN_EXPORT VulkanFunctionPointers* GetVulkanFunctionPointers();

struct VulkanFunctionPointers {
  VulkanFunctionPointers();
  ~VulkanFunctionPointers();

  // This function assumes that vkGetDeviceProcAddr has been populated.
  bool BindDeviceFunctionPointers(VkDevice vk_device);

  base::NativeLibrary vulkan_loader_library_ = nullptr;

  // Unassociated functions
  PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = nullptr;
  PFN_vkGetDeviceProcAddr vkGetDeviceProcAddr = nullptr;
  PFN_vkEnumerateInstanceExtensionProperties
      vkEnumerateInstanceExtensionProperties = nullptr;
  PFN_vkEnumerateInstanceLayerProperties vkEnumerateInstanceLayerProperties =
      nullptr;
  PFN_vkCreateInstance vkCreateInstance = nullptr;

  // Instance functions
  PFN_vkEnumeratePhysicalDevices vkEnumeratePhysicalDevices = nullptr;
  PFN_vkDestroySurfaceKHR vkDestroySurfaceKHR = nullptr;
  PFN_vkDestroyInstance vkDestroyInstance = nullptr;

  // Physical Device functions
  PFN_vkGetPhysicalDeviceQueueFamilyProperties
      vkGetPhysicalDeviceQueueFamilyProperties = nullptr;
  PFN_vkEnumerateDeviceLayerProperties vkEnumerateDeviceLayerProperties =
      nullptr;
  PFN_vkCreateDevice vkCreateDevice = nullptr;
  PFN_vkGetPhysicalDeviceSurfaceSupportKHR
      vkGetPhysicalDeviceSurfaceSupportKHR = nullptr;
  PFN_vkGetPhysicalDeviceSurfaceFormatsKHR
      vkGetPhysicalDeviceSurfaceFormatsKHR = nullptr;
  PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR
      vkGetPhysicalDeviceSurfaceCapabilitiesKHR = nullptr;

  // Device functions
""")

  for func in device_functions:
    file.write('  PFN_' + func['name'] + ' ' + func['name'] + ' = nullptr;\n')

  file.write("""\

  // Queue functions
""")

  for func in queue_functions:
    file.write('  PFN_' + func['name'] + ' ' + func['name'] + ' = nullptr;\n')

  file.write("""\

  // Command Buffer functions
""")

  for func in command_buffer_functions:
    file.write('  PFN_' + func['name'] + ' ' + func['name'] + ' = nullptr;\n')

  file.write("""\
};

}  // namespace gpu

#endif  // GPU_VULKAN_VULKAN_FUNCTION_POINTERS_H_""")

def GenerateSourceFile(file, device_functions, queue_functions,
                       command_buffer_functions):
  """Generates gpu/vulkan/vulkan_function_pointers.cc"""

  file.write(LICENSE_AND_HEADER +
"""

#include "gpu/vulkan/vulkan_function_pointers.h"

#include "base/no_destructor.h"

namespace gpu {

VulkanFunctionPointers* GetVulkanFunctionPointers() {
  static base::NoDestructor<VulkanFunctionPointers> vulkan_function_pointers;
  return vulkan_function_pointers.get();
}

VulkanFunctionPointers::VulkanFunctionPointers() = default;
VulkanFunctionPointers::~VulkanFunctionPointers() = default;

bool VulkanFunctionPointers::BindDeviceFunctionPointers(VkDevice vk_device) {

  // Device functions
""")
  for func in device_functions:
    file.write('  ' + func['name'] + ' = reinterpret_cast<PFN_' + func['name'] +
      '>(vkGetDeviceProcAddr(vk_device, "' + func['name'] + '"));\n')
    file.write('  if (!' + func['name'] + ')\n')
    file.write('    return false;\n\n')

  file.write("""\

  // Queue functions
""")
  for func in queue_functions:
    file.write('  ' + func['name'] + ' = reinterpret_cast<PFN_' + func['name'] +
      '>(vkGetDeviceProcAddr(vk_device, "' + func['name'] + '"));\n')
    file.write('  if (!' + func['name'] + ')\n')
    file.write('    return false;\n\n')

  file.write("""\

  // Command Buffer functions
""")
  for func in command_buffer_functions:
    file.write('  ' + func['name'] + ' = reinterpret_cast<PFN_' + func['name'] +
      '>(vkGetDeviceProcAddr(vk_device, "' + func['name'] + '"));\n')
    file.write('  if (!' + func['name'] + ')\n')
    file.write('    return false;\n\n')

  file.write("""\


  return true;
}

}  // namespace gpu""")

def main(argv):
  """This is the main function."""

  parser = optparse.OptionParser()
  _, args = parser.parse_args(argv)

  directory = SELF_LOCATION
  if len(args) >= 1:
    directory = args[0]

  def ClangFormat(filename):
    formatter = "clang-format"
    if platform.system() == "Windows":
      formatter += ".bat"
    call([formatter, "-i", "-style=chromium", filename])

  header_file = open(
      os.path.join(directory, 'vulkan_function_pointers.h'), 'wb')
  GenerateHeaderFile(header_file, VULKAN_DEVICE_FUNCTIONS,
                     VULKAN_QUEUE_FUNCTIONS, VULKAN_COMMAND_BUFFER_FUNCTIONS)
  header_file.close()
  ClangFormat(header_file.name)

  source_file = open(
      os.path.join(directory, 'vulkan_function_pointers.cc'), 'wb')
  GenerateSourceFile(source_file, VULKAN_DEVICE_FUNCTIONS,
                     VULKAN_QUEUE_FUNCTIONS, VULKAN_COMMAND_BUFFER_FUNCTIONS)
  source_file.close()
  ClangFormat(source_file.name)

  return 0

if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
