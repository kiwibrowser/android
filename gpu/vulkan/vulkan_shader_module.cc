// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/vulkan/vulkan_shader_module.h"

#include <memory>
#include <sstream>

#include "base/logging.h"
#include "gpu/vulkan/vulkan_device_queue.h"
#include "gpu/vulkan/vulkan_function_pointers.h"

namespace gpu {

VulkanShaderModule::VulkanShaderModule(VulkanDeviceQueue* device_queue)
    : device_queue_(device_queue) {
  DCHECK(device_queue_);
}

VulkanShaderModule::~VulkanShaderModule() {
  DCHECK_EQ(static_cast<VkShaderModule>(VK_NULL_HANDLE), handle_);
}

bool VulkanShaderModule::InitializeSPIRV(ShaderType type,
                                         std::string name,
                                         std::string entry_point,
                                         std::string source) {
  DCHECK_EQ(static_cast<VkShaderModule>(VK_NULL_HANDLE), handle_);
  VulkanFunctionPointers* vulkan_function_pointers =
      gpu::GetVulkanFunctionPointers();
  shader_type_ = type;
  name_ = std::move(name);
  entry_point_ = std::move(entry_point);

  // Make sure source is a multiple of 4.
  const int padding = 4 - (source.length() % 4);
  if (padding < 4) {
    for (int i = 0; i < padding; ++i) {
      source += ' ';
    }
  }

  VkShaderModuleCreateInfo shader_module_create_info = {};
  shader_module_create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  shader_module_create_info.pCode =
      reinterpret_cast<const uint32_t*>(source.c_str());
  shader_module_create_info.codeSize = source.length();

  VkShaderModule shader_module = VK_NULL_HANDLE;
  VkResult result = vulkan_function_pointers->vkCreateShaderModule(
      device_queue_->GetVulkanDevice(), &shader_module_create_info, nullptr,
      &shader_module);
  if (VK_SUCCESS != result) {
    std::stringstream ss;
    ss << "vkCreateShaderModule() failed: " << result;
    error_messages_ = ss.str();
    DLOG(ERROR) << error_messages_;
    return false;
  }

  handle_ = shader_module;
  return true;
}

void VulkanShaderModule::Destroy() {
  if (handle_ != VK_NULL_HANDLE) {
    VulkanFunctionPointers* vulkan_function_pointers =
        gpu::GetVulkanFunctionPointers();
    vulkan_function_pointers->vkDestroyShaderModule(
        device_queue_->GetVulkanDevice(), handle_, nullptr);
    handle_ = VK_NULL_HANDLE;
  }

  entry_point_.clear();
  error_messages_.clear();
}

}  // namespace gpu
